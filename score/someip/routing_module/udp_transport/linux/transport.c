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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DATAGRAM_SIZE 65536U
#define MIN_HEADER_SIZE 16U
#define MAX_DATAGRAMS_PER_DRAIN 32U

static struct pollfd* poll_descriptors;
static unsigned short* socket_ports;
static size_t descriptor_count;
static int wake_descriptor = -1;

int someip_linux_transport_init(void)
{
    if (wake_descriptor >= 0)
    {
        return EALREADY;
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
        if (socket_ports[index] == port)
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
    unsigned short* new_ports = (unsigned short*)realloc(socket_ports, (descriptor_count + 1U) * sizeof(*socket_ports));
    if (new_ports == NULL)
    {
        (void)close(descriptor);
        return ENOMEM;
    }
    socket_ports = new_ports;
    poll_descriptors[descriptor_count + 1U].fd = descriptor;
    poll_descriptors[descriptor_count + 1U].events = POLLIN;
    poll_descriptors[descriptor_count + 1U].revents = 0;
    socket_ports[descriptor_count] = port;
    ++descriptor_count;
    return 0;
}

void someip_transport_copy_packet(const void* packet, unsigned int offset, unsigned int size, void* destination)
{
    if (size != 0U)
    {
        (void)memcpy(destination, (const unsigned char*)packet + offset, size);
    }
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

static int drain_socket(size_t index)
{
    for (unsigned int count = 0; count < MAX_DATAGRAMS_PER_DRAIN; ++count)
    {
        unsigned char packet[MAX_DATAGRAM_SIZE];
        struct sockaddr_in sender;
        (void)memset(&sender, 0, sizeof(sender));
        socklen_t sender_size = sizeof(sender);
        const ssize_t received = recvfrom(poll_descriptors[index + 1U].fd,
                                          packet,
                                          sizeof(packet),
                                          MSG_DONTWAIT,
                                          (struct sockaddr*)&sender,
                                          &sender_size);
        if (received < 0)
        {
            return (errno == EAGAIN) ? 0 : errno;
        }
        if (received < MIN_HEADER_SIZE)
        {
            someip_log(SOMEIP_LOG_WARNING, "short datagram on port %u", socket_ports[index]);
            continue;
        }
        someip_routing_datagram(socket_ports[index],
                                (const unsigned char*)&sender.sin_addr.s_addr,
                                ntohs(sender.sin_port),
                                packet,
                                (unsigned int)received);
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
        poll_descriptors = (struct pollfd*)malloc(sizeof(*poll_descriptors));
        if (poll_descriptors == NULL)
        {
            return ENOMEM;
        }
    }
    poll_descriptors[0].fd = wake_descriptor;
    poll_descriptors[0].events = POLLIN;
    poll_descriptors[0].revents = 0;
    const int result = poll(poll_descriptors, descriptor_count + 1U, timeout_ms);
    if (result < 0)
    {
        return (errno == EINTR) ? 0 : errno;
    }
    if (((unsigned int)poll_descriptors[0].revents & (unsigned int)POLLIN) != 0U)
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
    for (size_t index = 0; index < descriptor_count; ++index)
    {
        if (((unsigned int)poll_descriptors[index + 1U].revents & (unsigned int)POLLIN) == 0U)
        {
            continue;
        }
        const int error = drain_socket(index);
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
        (void)close(poll_descriptors[index + 1U].fd);
    }
    free(poll_descriptors);
    free(socket_ports);
    poll_descriptors = NULL;
    socket_ports = NULL;
    descriptor_count = 0U;
    if (wake_descriptor >= 0)
    {
        (void)close(wake_descriptor);
        wake_descriptor = -1;
    }
}
