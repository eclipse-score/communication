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
#include "score/someip/routing_module/udp_transport/udp_transport.h"

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

#include "score/someip/routing_module/udp_transport/log.h"
#include "score/someip/routing_module/udp_transport/qnx/transport.h"
#include "score/someip/routing_module/udp_transport/qnx/wakeup_signal.h"

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
static someip_log_limiter short_datagram_limiter;
static someip_log_limiter truncated_datagram_limiter;

static int receive_upcall(struct socket* so __unused, void* arg, int waitflag __unused)
{
    struct event_socket* entry = arg;
    /* Runs in the network stack with the receive buffer locked, so it only marks and wakes. */
    atomic_store_rel_int(&entry->rx_pending, 1U);
    someip_transport_wakeup();
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

int someip_transport_open_udp_socket(unsigned short port)
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

int someip_transport_copy_packet(const void* packet, unsigned int offset, unsigned int size, void* destination)
{
    m_copydata((const struct mbuf*)packet, (int)offset, (int)size, (char*)destination);
    return 0;
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
            uint64_t suppressed = 0U;
            if (someip_log_limiter_check(&truncated_datagram_limiter, &suppressed))
            {
                someip_log(SOMEIP_LOG_WARNING,
                           "truncated datagram on port %u (suppressed %llu)",
                           entry->port,
                           (unsigned long long)suppressed);
            }
        }
        else if ((ret == 0) && (length < SOMEIP_HEADER_SIZE))
        {
            uint64_t suppressed = 0U;
            if (someip_log_limiter_check(&short_datagram_limiter, &suppressed))
            {
                someip_log(SOMEIP_LOG_WARNING,
                           "short datagram on port %u (suppressed %llu)",
                           entry->port,
                           (unsigned long long)suppressed);
            }
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

void someip_qnx_transport_run(void* arg __unused)
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
                someip_transport_wakeup();
            }
        }
        someip_wakeup_wait(WAKEUP_TIMEOUT_MS);
    }
}
