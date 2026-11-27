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

#include "score/someip/routing_module/udp_transport/linux/transport.h"
#include "score/someip/routing_module/udp_transport/log.h"
#include "score/someip/routing_module/udp_transport/udp_transport.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MIN_HEADER_SIZE 16U
#define MAX_DATAGRAMS_PER_DRAIN 32U
#define RETRY_DELAY_NS 100000000ULL
#define NANOSECONDS_PER_MILLISECOND 1000000ULL

struct socket_state
{
    unsigned short port;
    int descriptor;
    uint64_t retry_after_ns;
    int pending_consume;
};

static struct pollfd* poll_descriptors;
static struct socket_state* socket_states;
static size_t descriptor_count;
static int wake_descriptor = -1;
static someip_log_limiter short_datagram_limiter;

static int is_recoverable_receive_error(int error)
{
    return (error == ECONNREFUSED) || (error == ECONNRESET) || (error == EHOSTUNREACH) || (error == ENETUNREACH);
}

static int monotonic_time_ns(uint64_t* now_ns)
{
    struct timespec now = {0, 0};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        return errno;
    }
    *now_ns = ((uint64_t)now.tv_sec * 1000000000ULL) + (uint64_t)now.tv_nsec;
    return 0;
}

int someip_linux_transport_init(void)
{
    if (wake_descriptor >= 0)
    {
        return EALREADY;
    }
    const int probe_descriptor = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (probe_descriptor < 0)
    {
        return errno;
    }
    const int offset = 0;
    const int result = setsockopt(probe_descriptor, SOL_SOCKET, SO_PEEK_OFF, &offset, sizeof(offset));
    const int error = (result == 0) ? 0 : errno;
    (void)close(probe_descriptor);
    if (error != 0)
    {
        return error;
    }
    wake_descriptor = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    return (wake_descriptor >= 0) ? 0 : errno;
}

int someip_transport_open_udp_socket(unsigned short port)
{
    if (wake_descriptor < 0)
    {
        return EINVAL;
    }
    for (size_t index = 0; index < descriptor_count; ++index)
    {
        if (socket_states[index].port == port)
        {
            return 0;
        }
    }
    const int descriptor = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (descriptor < 0)
    {
        return errno;
    }
    struct sockaddr_in address;
    (void)memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(descriptor, (const struct sockaddr*)&address, sizeof(address)) != 0)
    {
        const int error = errno;
        (void)close(descriptor);
        return error;
    }
    struct pollfd* new_descriptors =
        (struct pollfd*)realloc(poll_descriptors, (descriptor_count + 2U) * sizeof(*poll_descriptors));
    if (new_descriptors == NULL)
    {
        (void)close(descriptor);
        return ENOMEM;
    }
    poll_descriptors = new_descriptors;
    struct socket_state* new_states =
        (struct socket_state*)realloc(socket_states, (descriptor_count + 1U) * sizeof(*socket_states));
    if (new_states == NULL)
    {
        (void)close(descriptor);
        return ENOMEM;
    }
    socket_states = new_states;
    poll_descriptors[descriptor_count + 1U].fd = descriptor;
    poll_descriptors[descriptor_count + 1U].events = POLLIN;
    poll_descriptors[descriptor_count + 1U].revents = 0;
    socket_states[descriptor_count].port = port;
    socket_states[descriptor_count].descriptor = descriptor;
    socket_states[descriptor_count].retry_after_ns = 0U;
    socket_states[descriptor_count].pending_consume = 0;
    ++descriptor_count;
    return 0;
}

int someip_transport_copy_packet(const void* packet, unsigned int offset, unsigned int size, void* destination)
{
    if (size == 0U)
    {
        return 0;
    }
    if (offset > INT_MAX)
    {
        return EINVAL;
    }
    const int descriptor = *(const int*)packet;
    const int peek_offset = (int)offset;
    if (setsockopt(descriptor, SOL_SOCKET, SO_PEEK_OFF, &peek_offset, sizeof(peek_offset)) != 0)
    {
        return errno;
    }
    ssize_t received = 0;
    do
    {
        received = recv(descriptor, destination, size, MSG_PEEK | MSG_DONTWAIT);
    } while ((received < 0) && (errno == EINTR));
    if (received < 0)
    {
        return errno;
    }
    return ((size_t)received == size) ? 0 : EIO;
}

void someip_transport_wakeup(void)
{
    if (wake_descriptor >= 0)
    {
        const uint64_t signal = 1U;
        ssize_t written = 0;
        do
        {
            written = write(wake_descriptor, &signal, sizeof(signal));
        } while ((written < 0) && (errno == EINTR));
        if ((written < 0) && (errno != EAGAIN))
        {
            someip_log(SOMEIP_LOG_ERROR, "waking the Linux transport failed: %d", errno);
        }
    }
}

static int dispatch_datagram(size_t index, int descriptor)
{
    struct sockaddr_in sender;
    (void)memset(&sender, 0, sizeof(sender));
    socklen_t sender_size = sizeof(sender);
    const int peek_offset = 0;
    if (setsockopt(descriptor, SOL_SOCKET, SO_PEEK_OFF, &peek_offset, sizeof(peek_offset)) != 0)
    {
        return errno;
    }
    ssize_t received = 0;
    do
    {
        received = recvfrom(
            descriptor, NULL, 0U, MSG_PEEK | MSG_TRUNC | MSG_DONTWAIT, (struct sockaddr*)&sender, &sender_size);
    } while ((received < 0) && (errno == EINTR));
    if (received < 0)
    {
        return errno;
    }
    if (received < MIN_HEADER_SIZE)
    {
        uint64_t suppressed = 0U;
        if (someip_log_limiter_check(&short_datagram_limiter, &suppressed))
        {
            someip_log(SOMEIP_LOG_WARNING,
                       "short datagram on port %u (suppressed %llu)",
                       socket_states[index].port,
                       (unsigned long long)suppressed);
        }
    }
    else
    {
        someip_routing_datagram(socket_states[index].port,
                                (const unsigned char*)&sender.sin_addr.s_addr,
                                ntohs(sender.sin_port),
                                &descriptor,
                                (unsigned int)received);
    }
    socket_states[index].pending_consume = 1;
    return 0;
}

static int drain_socket(size_t index)
{
    const int descriptor = poll_descriptors[index + 1U].fd;
    for (unsigned int count = 0; count < MAX_DATAGRAMS_PER_DRAIN; ++count)
    {
        if (!socket_states[index].pending_consume)
        {
            const int error = dispatch_datagram(index, descriptor);
            if (error != 0)
            {
                return (error == EAGAIN) ? 0 : error;
            }
        }
        ssize_t consumed = 0;
        do
        {
            consumed = recv(descriptor, NULL, 0U, MSG_TRUNC | MSG_DONTWAIT);
        } while ((consumed < 0) && (errno == EINTR));
        if (consumed < 0)
        {
            return (errno == EAGAIN) ? EIO : errno;
        }
        socket_states[index].pending_consume = 0;
    }
    return 0;
}

static int update_retry_deadlines(int* timeout_ms)
{
    for (size_t index = 0; index < descriptor_count; ++index)
    {
        if (poll_descriptors[index + 1U].fd >= 0)
        {
            continue;
        }
        uint64_t now_ns = 0U;
        const int time_error = monotonic_time_ns(&now_ns);
        if (time_error != 0)
        {
            return time_error;
        }
        if (now_ns >= socket_states[index].retry_after_ns)
        {
            poll_descriptors[index + 1U].fd = socket_states[index].descriptor;
            continue;
        }
        const uint64_t delay_ns = socket_states[index].retry_after_ns - now_ns;
        const int delay_ms = (int)((delay_ns + NANOSECONDS_PER_MILLISECOND - 1U) / NANOSECONDS_PER_MILLISECOND);
        if ((*timeout_ms < 0) || (delay_ms < *timeout_ms))
        {
            *timeout_ms = delay_ms;
        }
    }
    return 0;
}

static int process_wakeup(void)
{
    const unsigned int events = (unsigned int)poll_descriptors[0].revents;
    if ((events & (unsigned int)POLLNVAL) != 0U)
    {
        return EBADF;
    }
    if ((events & ((unsigned int)POLLERR | (unsigned int)POLLHUP)) != 0U)
    {
        return EIO;
    }
    if ((events & (unsigned int)POLLIN) != 0U)
    {
        uint64_t signal = 0U;
        ssize_t received = 0;
        do
        {
            received = read(wake_descriptor, &signal, sizeof(signal));
        } while ((received < 0) && (errno == EINTR));
        if (received < 0)
        {
            return errno;
        }
        someip_routing_poll();
    }
    return 0;
}

static int process_socket(size_t index)
{
    const unsigned int events = (unsigned int)poll_descriptors[index + 1U].revents;
    int recoverable_error = 0;
    if ((events & (unsigned int)POLLNVAL) != 0U)
    {
        return EBADF;
    }
    if ((events & (unsigned int)POLLHUP) != 0U)
    {
        return EIO;
    }
    if ((events & (unsigned int)POLLERR) != 0U)
    {
        int socket_error = 0;
        socklen_t error_size = sizeof(socket_error);
        if (getsockopt(poll_descriptors[index + 1U].fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_size) != 0)
        {
            return errno;
        }
        if (!is_recoverable_receive_error(socket_error))
        {
            return (socket_error != 0) ? socket_error : EIO;
        }
        recoverable_error = socket_error;
    }
    if ((events & (unsigned int)POLLIN) != 0U)
    {
        const int error = drain_socket(index);
        if (error != 0)
        {
            if (!is_recoverable_receive_error(error))
            {
                return error;
            }
            recoverable_error = error;
        }
    }
    if (recoverable_error != 0)
    {
        uint64_t now_ns = 0U;
        const int time_error = monotonic_time_ns(&now_ns);
        if (time_error != 0)
        {
            return time_error;
        }
        someip_log(SOMEIP_LOG_ERROR, "UDP receive on port %u failed: %d", socket_states[index].port, recoverable_error);
        socket_states[index].retry_after_ns = now_ns + RETRY_DELAY_NS;
        poll_descriptors[index + 1U].fd = -1;
    }
    return 0;
}

int someip_linux_transport_poll(int timeout_ms)
{
    if (wake_descriptor < 0)
    {
        return EINVAL;
    }
    someip_routing_poll();
    if (poll_descriptors == NULL)
    {
        if (descriptor_count != 0U)
        {
            return EINVAL;
        }
        poll_descriptors = (struct pollfd*)malloc(sizeof(*poll_descriptors));
        if (poll_descriptors == NULL)
        {
            return ENOMEM;
        }
    }
    poll_descriptors[0].fd = wake_descriptor;
    poll_descriptors[0].events = POLLIN;
    poll_descriptors[0].revents = 0;
    const int time_error = update_retry_deadlines(&timeout_ms);
    if (time_error != 0)
    {
        return time_error;
    }
    const int result = poll(poll_descriptors, descriptor_count + 1U, timeout_ms);
    if (result < 0)
    {
        return (errno == EINTR) ? 0 : errno;
    }
    const int wakeup_error = process_wakeup();
    if (wakeup_error != 0)
    {
        return wakeup_error;
    }
    for (size_t index = 0; index < descriptor_count; ++index)
    {
        const int error = process_socket(index);
        if (error != 0)
        {
            return error;
        }
    }
    return 0;
}

void someip_linux_transport_close(void)
{
    for (size_t index = 0; index < descriptor_count; ++index)
    {
        (void)close(socket_states[index].descriptor);
    }
    free(poll_descriptors);
    free(socket_states);
    poll_descriptors = NULL;
    socket_states = NULL;
    descriptor_count = 0U;
    (void)memset(&short_datagram_limiter, 0, sizeof(short_datagram_limiter));
    if (wake_descriptor >= 0)
    {
        (void)close(wake_descriptor);
        wake_descriptor = -1;
    }
}
