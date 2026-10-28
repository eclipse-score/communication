/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#include <cstring>
#include <atomic>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <socket/IP4Address.h>
#include <socket/IP6Address.h>
#include <socket/NetworkDevice.h>
#include <socket/UDPSocket.h>
#include <thread>
#include <sys/time.h>

#include "Helper.h"

using namespace lgsomeip::osabstraction;

constexpr int kUdpMax = 5000;
char udp_message[kUdpMax];
int count = 0;
std::atomic<bool> udp_client_message_matches{false};
std::string multicast_ip6 = "FF02::1:FF00";

void udp_client_run() {
    usleep(10);
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_ip_address("127.0.0.1");
    addr->set_reliable(false);
    addr->set_port_address(get_test_port());

    UDPSocket client(addr);

    char receive_message[kUdpMax];
    client.send(udp_message, sizeof(udp_message), addr);

    int received_length = client.receive(receive_message, sizeof(receive_message), addr);
    udp_client_message_matches = received_length == sizeof(receive_message) &&
                                 std::memcmp(udp_message, receive_message, sizeof(receive_message)) == 0;
}

void multicast_sender_ipv4() {
    usleep(10);
    std::shared_ptr<Address> address = std::make_shared<IP4Address>();
    address->set_reliable(false);
    address->set_port_address(get_test_port(1)); // Multicast Port
    address->set_ip_address("224.0.0.1");        // Multicast IP

    UDPSocket sender(address);
    std::uint32_t ttl = 64;
    sender.set_multicast_ttl(ttl);

    sender.send(udp_message, sizeof(udp_message), address);
}

void multicast_sender_ipv6() {
    usleep(10);
    std::shared_ptr<Address> address = std::make_shared<IP6Address>();
    address->set_reliable(false);
    address->set_port_address(get_test_port(2)); // Multicast Port
    address->set_ip_address(multicast_ip6);      // Multicast IP

    UDPSocket sender(address);
    std::uint32_t ttl = 64;
    sender.set_multicast_ttl(ttl);

    sender.send(udp_message, sizeof(udp_message), address);
}

TEST(UDPSocketTest, UDPSocketTest) {
    char message[kUdpMax];
    create_msg(udp_message, kUdpMax);
    std::shared_ptr<std::thread> udp_thread, sender_thread, receiver_thread;

    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_reliable(false);
    addr->set_port_address(get_test_port());

    UDPSocket server(addr);
    server.bind();

    udp_thread = std::make_shared<std::thread>(udp_client_run);

    // unconnected udp
    std::shared_ptr<Address> client_addr = std::make_shared<IP4Address>();
    client_addr->set_reliable(false);
    addr->set_ip_address("127.0.0.1");

    int received_length = server.receive(message, sizeof(message), client_addr);
    ASSERT_EQ(received_length, sizeof(message));
    ASSERT_STREQ(udp_message, message);

    ASSERT_NE(client_addr->get_address(), nullptr);
    struct sockaddr_in* address = reinterpret_cast<struct sockaddr_in*>(client_addr->get_address());
    std::cout << "Client IP : " << inet_ntoa(address->sin_addr) << std::endl;
    std::cout << "Client PORT : " << ntohs(address->sin_port) << std::endl;

    server.send(udp_message, sizeof(udp_message), client_addr);

    udp_thread->join();
    ASSERT_TRUE(udp_client_message_matches);
}

TEST(UDPSocketTest, UDPMulticastIPv4) {
    char multicast_message[kUdpMax];
    std::shared_ptr<std::thread> sender_thread;
    std::string multicast_ip = "224.0.0.1";

    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_reliable(false);
    addr->set_port_address(get_test_port(1));

    UDPSocket receiver(addr);
    receiver.bind();

    if (!receiver.join_multicast(multicast_ip)) {
        GTEST_SKIP() << "IPv4 multicast is unavailable on this host";
    }
    timeval receive_timeout{1, 0};
    setsockopt(receiver.get_socket_fd(), SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));
    std::shared_ptr<Address> client_addr = std::make_shared<IP4Address>();

    sender_thread = std::make_shared<std::thread>(multicast_sender_ipv4);
    int received_length = receiver.receive(multicast_message, sizeof(multicast_message), client_addr);
    sender_thread->join();
    if (received_length < 0) {
        GTEST_SKIP() << "IPv4 multicast packets are unavailable on this host";
    }
    ASSERT_STREQ(udp_message, multicast_message);
    receiver.leave_multicast(multicast_ip);
}

TEST(UDPSocketTest, UDPMulticastIPv6) {
    char multicast_message[kUdpMax];
    create_msg(udp_message, kUdpMax);
    std::shared_ptr<std::thread> sender_thread;

    if (!NetworkDevice::instance().initialize("::1", 6)) {
        GTEST_SKIP() << "IPv6 loopback interface is unavailable on this host";
    }

    std::shared_ptr<Address> addr = std::make_shared<IP6Address>();
    addr->set_reliable(false);
    addr->set_port_address(get_test_port(2));

    UDPSocket receiver(addr);
    receiver.bind();

    if (!receiver.join_multicast(multicast_ip6)) {
        GTEST_SKIP() << "IPv6 multicast is unavailable on this host";
    }
    std::uint32_t on = 1;
    receiver.set_multicast_loop(on);
    timeval receive_timeout{1, 0};
    setsockopt(receiver.get_socket_fd(), SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));

    std::shared_ptr<Address> client_addr = std::make_shared<IP6Address>();

    sender_thread = std::make_shared<std::thread>(multicast_sender_ipv6);
    int received_length = receiver.receive(multicast_message, sizeof(multicast_message), client_addr);
    sender_thread->join();
    if (received_length < 0) {
        GTEST_SKIP() << "IPv6 multicast packets are unavailable on this host";
    }
    ASSERT_STREQ(udp_message, multicast_message);
    receiver.leave_multicast(multicast_ip6);
}
