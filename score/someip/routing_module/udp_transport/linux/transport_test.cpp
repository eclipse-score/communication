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

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <future>
#include <vector>

namespace
{
std::vector<unsigned char> received_packet;
std::array<unsigned char, 4U> received_tail;
std::array<unsigned char, 4U> received_address;
unsigned short received_local_port;
unsigned short received_sender_port;
unsigned int poll_calls;
unsigned int log_calls;
unsigned int received_count;
int receive_error;
int consume_error;
short poll_event;
int pending_socket_error;

unsigned short BoundPort(const int descriptor)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(descriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
    {
        return 0U;
    }
    socklen_t length = sizeof(address);
    return (::getsockname(descriptor, reinterpret_cast<sockaddr*>(&address), &length) == 0) ? ntohs(address.sin_port)
                                                                                            : 0U;
}
}  // namespace

extern "C" ssize_t __real_recvfrom(int, void*, size_t, int, struct sockaddr*, socklen_t*);
extern "C" ssize_t __real_recv(int, void*, size_t, int);
extern "C" int __real_poll(struct pollfd*, nfds_t, int);
extern "C" int __real_getsockopt(int, int, int, void*, socklen_t*);

extern "C" int __wrap_poll(struct pollfd* const descriptors, const nfds_t count, const int timeout)
{
    if (poll_event != 0)
    {
        descriptors[count - 1U].revents = poll_event;
        poll_event = 0;
        return 1;
    }
    return __real_poll(descriptors, count, timeout);
}

extern "C" int __wrap_getsockopt(const int descriptor,
                                 const int level,
                                 const int option,
                                 void* const value,
                                 socklen_t* const size)
{
    if ((level == SOL_SOCKET) && (option == SO_ERROR) && (pending_socket_error != 0))
    {
        *static_cast<int*>(value) = pending_socket_error;
        pending_socket_error = 0;
        return 0;
    }
    return __real_getsockopt(descriptor, level, option, value, size);
}

extern "C" ssize_t __wrap_recvfrom(const int descriptor,
                                   void* const buffer,
                                   const size_t size,
                                   const int flags,
                                   struct sockaddr* const address,
                                   socklen_t* const address_size)
{
    if (receive_error != 0)
    {
        errno = receive_error;
        receive_error = 0;
        return -1;
    }
    return __real_recvfrom(descriptor, buffer, size, flags, address, address_size);
}

extern "C" ssize_t __wrap_recv(const int descriptor, void* const buffer, const size_t size, const int flags)
{
    if (((flags & MSG_PEEK) == 0) && (consume_error != 0))
    {
        errno = consume_error;
        consume_error = 0;
        return -1;
    }
    return __real_recv(descriptor, buffer, size, flags);
}

extern "C" void someip_routing_log(someip_log_severity, const char*)
{
    ++log_calls;
}
extern "C" void someip_routing_poll(void)
{
    ++poll_calls;
}
extern "C" void someip_routing_datagram(const unsigned short local_port,
                                        const unsigned char sender_ip[4],
                                        const unsigned short sender_port,
                                        const void* const packet,
                                        const unsigned int size)
{
    received_local_port = local_port;
    received_sender_port = sender_port;
    std::copy_n(sender_ip, received_address.size(), received_address.begin());
    ++received_count;
    const auto tail_size = static_cast<unsigned int>(received_tail.size());
    EXPECT_EQ(someip_transport_copy_packet(packet, size - tail_size, tail_size, received_tail.data()), 0);
    received_packet.resize(size);
    EXPECT_EQ(someip_transport_copy_packet(packet, 0U, size, received_packet.data()), 0);
}

class LinuxTransportTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        received_packet.clear();
        poll_calls = 0U;
        log_calls = 0U;
        received_count = 0U;
        receive_error = 0;
        consume_error = 0;
        poll_event = 0;
        pending_socket_error = 0;
        ASSERT_EQ(someip_linux_transport_init(), 0);
    }

    void TearDown() override
    {
        someip_linux_transport_close();
    }
};

TEST_F(LinuxTransportTest, ReceivesDatagramAndPreservesEndpoint)
{
    const int reservation = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(reservation, 0);
    const auto local_port = BoundPort(reservation);
    ASSERT_NE(local_port, 0U);
    ::close(reservation);
    ASSERT_EQ(someip_transport_open_udp_socket(local_port), 0);
    EXPECT_EQ(someip_transport_open_udp_socket(local_port), 0);
    const int sender = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(sender, 0);
    const auto sender_port = BoundPort(sender);
    ASSERT_NE(sender_port, 0U);
    const std::array<unsigned char, 16U> datagram{1U, 2U, 3U, 4U};
    const std::array<unsigned char, 16U> next_datagram{
        5U, 6U, 7U, 8U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 9U, 10U, 11U, 12U};
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons(local_port);
    ASSERT_EQ(::sendto(sender,
                       datagram.data(),
                       datagram.size(),
                       0,
                       reinterpret_cast<const sockaddr*>(&destination),
                       sizeof(destination)),
              static_cast<ssize_t>(datagram.size()));
    ASSERT_EQ(::sendto(sender,
                       next_datagram.data(),
                       next_datagram.size(),
                       0,
                       reinterpret_cast<const sockaddr*>(&destination),
                       sizeof(destination)),
              static_cast<ssize_t>(next_datagram.size()));
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(received_count, 2U);
    EXPECT_EQ(received_local_port, local_port);
    EXPECT_EQ(received_sender_port, sender_port);
    EXPECT_EQ(received_address, (std::array<unsigned char, 4U>{127U, 0U, 0U, 1U}));
    EXPECT_EQ(received_packet, (std::vector<unsigned char>{next_datagram.cbegin(), next_datagram.cend()}));
    EXPECT_EQ(received_tail, (std::array<unsigned char, 4U>{9U, 10U, 11U, 12U}));
    ::close(sender);
}

TEST_F(LinuxTransportTest, ReportsPacketReadErrors)
{
    const int invalid_descriptor = -1;
    std::array<unsigned char, 4U> destination{};

    EXPECT_EQ(someip_transport_copy_packet(&invalid_descriptor, 0U, destination.size(), destination.data()), EBADF);
    EXPECT_EQ(someip_transport_copy_packet(&invalid_descriptor, 0U, 0U, nullptr), 0);
}

TEST(LogLimiterTest, ReportsSuppressedMessagesAfterInterval)
{
    someip_log_limiter limiter{};
    std::uint64_t suppressed{0U};

    EXPECT_TRUE(someip_log_limiter_at(&limiter, 0U, &suppressed));
    EXPECT_EQ(suppressed, 0U);
    EXPECT_FALSE(someip_log_limiter_at(&limiter, 1U, &suppressed));
    EXPECT_FALSE(someip_log_limiter_at(&limiter, 999999999U, &suppressed));
    EXPECT_TRUE(someip_log_limiter_at(&limiter, 1000000000U, &suppressed));
    EXPECT_EQ(suppressed, 2U);
    EXPECT_FALSE(someip_log_limiter_at(&limiter, 999999999U, &suppressed));
    EXPECT_TRUE(someip_log_limiter_at(&limiter, 2000000000U, &suppressed));
    EXPECT_EQ(suppressed, 1U);
}

TEST_F(LinuxTransportTest, ContinuesAfterTransientReceiveError)
{
    const int reservation = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(reservation, 0);
    const auto local_port = BoundPort(reservation);
    ASSERT_NE(local_port, 0U);
    ::close(reservation);
    ASSERT_EQ(someip_transport_open_udp_socket(local_port), 0);
    const int sender = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(sender, 0);
    const std::array<unsigned char, 16U> datagram{1U, 2U, 3U, 4U};
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons(local_port);
    ASSERT_EQ(::sendto(sender,
                       datagram.data(),
                       datagram.size(),
                       0,
                       reinterpret_cast<const sockaddr*>(&destination),
                       sizeof(destination)),
              static_cast<ssize_t>(datagram.size()));
    receive_error = ECONNREFUSED;

    EXPECT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(log_calls, 1U);
    EXPECT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(received_count, 1U);
    EXPECT_EQ(received_packet, (std::vector<unsigned char>{datagram.cbegin(), datagram.cend()}));
    ::close(sender);
}

TEST_F(LinuxTransportTest, ReportsInvalidWakeupDescriptor)
{
    poll_event = POLLNVAL;
    EXPECT_EQ(someip_linux_transport_poll(0), EBADF);
}

TEST_F(LinuxTransportTest, DoesNotForwardTwiceWhenConsumingDatagramFails)
{
    const int reservation = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(reservation, 0);
    const auto local_port = BoundPort(reservation);
    ASSERT_NE(local_port, 0U);
    ::close(reservation);
    ASSERT_EQ(someip_transport_open_udp_socket(local_port), 0);
    const int sender = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(sender, 0);
    const std::array<unsigned char, 16U> datagram{1U, 2U, 3U, 4U};
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons(local_port);
    ASSERT_EQ(::sendto(sender,
                       datagram.data(),
                       datagram.size(),
                       0,
                       reinterpret_cast<const sockaddr*>(&destination),
                       sizeof(destination)),
              static_cast<ssize_t>(datagram.size()));
    consume_error = ECONNREFUSED;

    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(received_count, 1U);
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(received_count, 1U);
    ::close(sender);
}

TEST_F(LinuxTransportTest, HandlesPendingSocketErrors)
{
    const int reservation = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(reservation, 0);
    const auto local_port = BoundPort(reservation);
    ASSERT_NE(local_port, 0U);
    ::close(reservation);
    ASSERT_EQ(someip_transport_open_udp_socket(local_port), 0);
    poll_event = POLLERR;
    pending_socket_error = EBADF;
    EXPECT_EQ(someip_linux_transport_poll(0), EBADF);

    poll_event = POLLERR;
    pending_socket_error = ECONNREFUSED;
    EXPECT_EQ(someip_linux_transport_poll(0), 0);
    EXPECT_EQ(log_calls, 1U);
}

TEST_F(LinuxTransportTest, ReceiveBackoffDoesNotBlockOtherSockets)
{
    const int first_reservation = ::socket(AF_INET, SOCK_DGRAM, 0);
    const int second_reservation = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(first_reservation, 0);
    ASSERT_GE(second_reservation, 0);
    const auto first_port = BoundPort(first_reservation);
    const auto second_port = BoundPort(second_reservation);
    ASSERT_NE(first_port, 0U);
    ASSERT_NE(second_port, 0U);
    ::close(first_reservation);
    ::close(second_reservation);
    ASSERT_EQ(someip_transport_open_udp_socket(first_port), 0);
    ASSERT_EQ(someip_transport_open_udp_socket(second_port), 0);
    const int sender = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(sender, 0);
    const std::array<unsigned char, 16U> datagram{1U, 2U, 3U, 4U};
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    for (const auto port : {first_port, second_port})
    {
        destination.sin_port = htons(port);
        ASSERT_EQ(::sendto(sender,
                           datagram.data(),
                           datagram.size(),
                           0,
                           reinterpret_cast<const sockaddr*>(&destination),
                           sizeof(destination)),
                  static_cast<ssize_t>(datagram.size()));
    }
    receive_error = ECONNREFUSED;
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(received_count, 1U);
    EXPECT_EQ(received_local_port, second_port);
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(received_count, 2U);
    EXPECT_EQ(received_local_port, first_port);
    ::close(sender);
}

TEST_F(LinuxTransportTest, WakeupProcessesPendingRoutingCommands)
{
    someip_transport_wakeup();
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(poll_calls, 2U);
}

TEST_F(LinuxTransportTest, WakeupFromAnotherThreadUnblocksPolling)
{
    auto result = std::async(std::launch::async, []() {
        return someip_linux_transport_poll(2000);
    });
    someip_transport_wakeup();
    EXPECT_EQ(result.wait_for(std::chrono::seconds{1}), std::future_status::ready);
    EXPECT_EQ(result.get(), 0);
    EXPECT_GE(poll_calls, 1U);
}

TEST_F(LinuxTransportTest, ReportsPortConflictsAndRepeatedInitialization)
{
    EXPECT_EQ(someip_linux_transport_init(), EALREADY);
    const int reservation = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(reservation, 0);
    const auto port = BoundPort(reservation);
    ASSERT_NE(port, 0U);
    EXPECT_EQ(someip_transport_open_udp_socket(port), EADDRINUSE);
    ::close(reservation);
}

TEST_F(LinuxTransportTest, LogsAndDropsDatagramsShorterThanAHeader)
{
    const int reservation = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(reservation, 0);
    const auto local_port = BoundPort(reservation);
    ASSERT_NE(local_port, 0U);
    ::close(reservation);
    ASSERT_EQ(someip_transport_open_udp_socket(local_port), 0);
    const int sender = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(sender, 0);
    const std::array<unsigned char, 3U> datagram{1U, 2U, 3U};
    const std::array<unsigned char, 16U> valid_datagram{4U, 5U, 6U, 7U};
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons(local_port);
    ASSERT_EQ(::sendto(sender, nullptr, 0U, 0, reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)),
              0);
    ASSERT_EQ(::sendto(sender,
                       datagram.data(),
                       datagram.size(),
                       0,
                       reinterpret_cast<const sockaddr*>(&destination),
                       sizeof(destination)),
              static_cast<ssize_t>(datagram.size()));
    ASSERT_EQ(::sendto(sender,
                       valid_datagram.data(),
                       valid_datagram.size(),
                       0,
                       reinterpret_cast<const sockaddr*>(&destination),
                       sizeof(destination)),
              static_cast<ssize_t>(valid_datagram.size()));
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(received_count, 1U);
    EXPECT_EQ(received_packet, (std::vector<unsigned char>{valid_datagram.cbegin(), valid_datagram.cend()}));
    EXPECT_EQ(log_calls, 1U);
    ::close(sender);
}
