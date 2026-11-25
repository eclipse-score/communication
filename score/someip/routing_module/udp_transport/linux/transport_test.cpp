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
std::array<unsigned char, 4U> received_address;
unsigned short received_local_port;
unsigned short received_sender_port;
unsigned int poll_calls;
unsigned int log_calls;

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
    received_packet.resize(size);
    someip_transport_copy_packet(packet, 0U, size, received_packet.data());
}

class LinuxTransportTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        received_packet.clear();
        poll_calls = 0U;
        log_calls = 0U;
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
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_EQ(received_local_port, local_port);
    EXPECT_EQ(received_sender_port, sender_port);
    EXPECT_EQ(received_address, (std::array<unsigned char, 4U>{127U, 0U, 0U, 1U}));
    EXPECT_EQ(received_packet, (std::vector<unsigned char>{datagram.cbegin(), datagram.cend()}));
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
    ASSERT_EQ(someip_linux_transport_poll(1000), 0);
    EXPECT_TRUE(received_packet.empty());
    EXPECT_EQ(log_calls, 1U);
    ::close(sender);
}
