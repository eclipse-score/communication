/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/someip/routing_module/iosock_transport/iosock_transport.h"

// clang-format off
/* io-sock's kernel headers require this include order. */
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <machine/atomic.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>
#include <qnx/qnx_modload.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/uio.h>
#include <netinet/in.h>
// clang-format on

#include "score/someip/routing_module/iosock_transport/log.h"
#include "score/someip/routing_module/iosock_transport/wakeup_signal.h"

MALLOC_DEFINE(M_SOMEIP_MODULE, "someip_module", "SOME/IP module");

#define SOMEIP_HEADER_SIZE 16U
/* Bounds the work per socket and wakeup, so a busy socket cannot starve routing work. */
#define MAX_DATAGRAMS_PER_DRAIN 32U
#define MAX_DATAGRAM_SIZE 65536U
/* Safety net only: every source of work wakes the kernel thread. */
#define WAKEUP_TIMEOUT_MS 1000U

/* The SDK only forward-declares struct uio; this is the FreeBSD 13.5 layout used by io-sock. */
enum uio_rw
{
    UIO_READ,
    UIO_WRITE
};

enum uio_seg
{
    UIO_USERSPACE,
    UIO_SYSSPACE,
    UIO_NOCOPY
};

struct uio
{
    struct iovec* uio_iov;
    int uio_iovcnt;
    off_t uio_offset;
    ssize_t uio_resid;
    enum uio_seg uio_segflg;
    enum uio_rw uio_rw;
    struct thread* uio_td;
};

struct event_socket
{
    unsigned short port;
    struct socket* socket;
    /* Set by the receive upcall, cleared by the kernel thread before draining. */
    unsigned int rx_pending;
    struct event_socket* next;
};

/* Only accessed by the kernel thread; sockets are never closed. */
static struct event_socket* sockets;

static int receive_upcall(struct socket* so __unused, void* arg, int waitflag __unused)
{
    struct event_socket* entry = arg;
    /* Runs in the network stack with the receive buffer locked, so it only marks and wakes. */
    atomic_store_rel_int(&entry->rx_pending, 1U);
    someip_iosock_wakeup();
    return SU_OK;
}

static struct event_socket* find_socket(unsigned short port)
{
    for (struct event_socket* entry = sockets; entry != NULL; entry = entry->next)
    {
        if (entry->port == port)
        {
            return entry;
        }
    }
    return NULL;
}

int someip_iosock_open_udp_socket(unsigned short port)
{
    if (find_socket(port) != NULL)
    {
        return 0;
    }
    struct event_socket* entry = malloc(sizeof(*entry), M_SOMEIP_MODULE, M_NOWAIT | M_ZERO);
    if (entry == NULL)
    {
        someip_log(SOMEIP_LOG_ERROR, "allocating the socket entry for port %u failed", port);
        return ENOMEM;
    }
    /* TODO: size SO_RCVBUF for event rate x drain latency; UDP drops silently on overflow. */
    int ret = socreate(AF_INET, &entry->socket, SOCK_DGRAM, IPPROTO_UDP, curthread->td_ucred, curthread);
    if (ret == 0)
    {
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_len = sizeof(address);
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);
        ret = sobind(entry->socket, (struct sockaddr*)&address, curthread);
        if (ret != 0)
        {
            soclose(entry->socket);
        }
    }
    if (ret != 0)
    {
        someip_log(SOMEIP_LOG_ERROR, "opening the UDP socket on port %u failed: %d", port, ret);
        free(entry, M_SOMEIP_MODULE);
        return ret;
    }
    entry->port = port;
    /* Datagrams may arrive before the upcall is installed; the next drain picks them up. */
    entry->rx_pending = 1U;
    SOCKBUF_LOCK(&entry->socket->so_rcv);
    soupcall_set(entry->socket, SO_RCV, receive_upcall, entry);
    SOCKBUF_UNLOCK(&entry->socket->so_rcv);
    entry->next = sockets;
    sockets = entry;
    return 0;
}

void someip_iosock_copy_packet(const void* packet, unsigned int offset, unsigned int size, void* destination)
{
    m_copydata((const struct mbuf*)packet, (int)offset, (int)size, (char*)destination);
}

/* Returns 1 if the budget was used up before the receive buffer was empty, 0 otherwise. */
static int drain_socket(struct event_socket* entry)
{
    for (unsigned int count = 0U; count < MAX_DATAGRAMS_PER_DRAIN; ++count)
    {
        struct uio receive;
        memset(&receive, 0, sizeof(receive));
        receive.uio_resid = MAX_DATAGRAM_SIZE;
        receive.uio_segflg = UIO_SYSSPACE;
        receive.uio_rw = UIO_READ;
        receive.uio_td = curthread;
        struct sockaddr* from = NULL;
        struct mbuf* packet = NULL;
        int flags = MSG_DONTWAIT;
        const int ret = soreceive(entry->socket, &from, &receive, &packet, NULL, &flags);
        const unsigned int length = (packet != NULL) ? m_length(packet, NULL) : 0U;
        if ((ret == 0) && (from != NULL) && ((flags & MSG_TRUNC) == 0) && (length >= SOMEIP_HEADER_SIZE))
        {
            const struct sockaddr_in* sender = (const struct sockaddr_in*)from;
            someip_routing_datagram(
                entry->port, (const unsigned char*)&sender->sin_addr.s_addr, ntohs(sender->sin_port), packet, length);
        }
        else if ((ret == 0) && ((flags & MSG_TRUNC) != 0))
        {
            someip_log(SOMEIP_LOG_WARNING, "truncated datagram on port %u", entry->port);
        }
        else if ((ret == 0) && (length < SOMEIP_HEADER_SIZE))
        {
            someip_log(SOMEIP_LOG_WARNING, "short datagram on port %u", entry->port);
        }
        if (packet != NULL)
        {
            m_freem(packet);
        }
        if (from != NULL)
        {
            free(from, M_SONAME);
        }
        if (ret != 0)
        {
            if ((ret != EWOULDBLOCK) && (ret != EAGAIN))
            {
                someip_log(SOMEIP_LOG_ERROR, "receiving on port %u failed: %d", entry->port, ret);
            }
            return 0;
        }
    }
    return 1;
}

static void kernel_thread(void* arg __unused)
{
    someip_log(SOMEIP_LOG_INFO, "kernel thread started (tid %d)", someip_thread_id());
    for (;;)
    {
        someip_routing_poll();
        for (struct event_socket* entry = sockets; entry != NULL; entry = entry->next)
        {
            /* Cleared before draining, so datagrams arriving meanwhile mark the socket again. */
            if ((atomic_readandclear_int(&entry->rx_pending) != 0U) && (drain_socket(entry) != 0))
            {
                atomic_store_rel_int(&entry->rx_pending, 1U);
                someip_iosock_wakeup();
            }
        }
        someip_wakeup_wait(WAKEUP_TIMEOUT_MS);
    }
}

static int module_handler(module_t module __unused, int what, void* arg __unused)
{
    switch (what)
    {
        case MOD_LOAD:
        {
            int error = someip_wakeup_init();
            if (error != 0)
            {
                someip_log(SOMEIP_LOG_ERROR, "wakeup initialization failed: %d", error);
                return error;
            }
            error = someip_routing_start();
            if (error != 0)
            {
                someip_log(SOMEIP_LOG_ERROR, "routing module start failed: %d", error);
                return error;
            }
            struct thread* thread = NULL;
            error = kthread_add(kernel_thread, NULL, NULL, &thread, 0, 0, "someip");
            if (error != 0)
            {
                someip_log(SOMEIP_LOG_ERROR, "kernel thread creation failed: %d", error);
                someip_routing_abort_start();
            }
            return error;
        }
        case MOD_QUIESCE:
        case MOD_UNLOAD:
            /* There is no coordinated kernel thread stop or mbuf drain, so unloading is refused. */
            return EOPNOTSUPP;
        case MOD_SHUTDOWN:
            return 0;
        default:
            return EOPNOTSUPP;
    }
}

static moduledata_t someip_moduledata = {"someip", module_handler, NULL};

MODULE_VERSION(someip, 1);

DECLARE_MODULE(someip, someip_moduledata, SI_SUB_PSEUDO, SI_ORDER_ANY);

/* Checked by the io-sock loader for API compatibility. */
struct _iosock_module_version iosock_module_version = IOSOCK_MODULE_VER_SYM_INIT;

static int someip_module_version = IOSOCK_VERSION_CUR;

SYSCTL_INT(_qnx_module, OID_AUTO, someip, CTLFLAG_RD, &someip_module_version, 0, "Version");

static void someip_module_uninit(void* arg __unused) {}

/* Linker-set placeholder from the QNX sample; SI_SUB_DUMMY never runs it. */
SYSUNINIT(someip_module_uninit, SI_SUB_DUMMY, SI_ORDER_ANY, someip_module_uninit, NULL);
