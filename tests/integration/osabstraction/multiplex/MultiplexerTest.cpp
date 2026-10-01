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

#include <endpoint/Endpoint.h>
#include <endpoint/EndpointTCP.h>
#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <multiplex/Multiplexer.h>
#include <mutex>
#include <socket/TCPClientSocket.h>
#include <socket/TCPServerSocket.h>
#include <socket/UDPSocket.h>
#include <thread>
#include "../socket/Helper.h"

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

std::map<int, std::shared_ptr<Endpoint>> endpoint_list = {};
std::shared_ptr<Multiplexer> mux = std::make_shared<Multiplexer>();

constexpr int kMaxLen = 5000;
char message[kMaxLen];
std::condition_variable callback_condition;
std::mutex callback_mutex;
int on_message_count = 0;
int tcp_message_count = 0;
int udp_message_count = 0;
int on_connect_count = 0;
bool messages_match = true;

class EndpointClient : public Endpoint {
public:
    using Endpoint::Endpoint;

    void callback(bool close) override {
        if (close) {
            return;
        }

        std::uint8_t received_message[kMaxLen] = {};
        int received_length = 0;
        if (get_socket()->get_reliable()) {
            std::size_t total_received = 0;
            for (int attempt = 0; attempt < 100 && total_received < sizeof(received_message); ++attempt) {
                int current_received = get_socket()->receive(reinterpret_cast<char*>(received_message) + total_received,
                                                             sizeof(received_message) - total_received);
                if (current_received < 0) {
                    return;
                }
                if (current_received == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                total_received += static_cast<std::size_t>(current_received);
            }
            received_length = static_cast<int>(total_received);
        } else {
            received_length = get_socket()->receive(reinterpret_cast<char*>(received_message), sizeof(received_message),
                                                    get_sender_address());
        }

        if (received_length > 0) {
            on_message(received_message, static_cast<std::size_t>(received_length));
        }
    }

    void on_message(std::uint8_t* message_data, std::size_t message_length) override {
        const bool message_matches =
            message_length == sizeof(message) && std::memcmp(message_data, message, sizeof(message)) == 0;
        std::lock_guard<std::mutex> lock(callback_mutex);
        ++on_message_count;
        messages_match = messages_match && message_matches;
        if (get_socket()->get_reliable()) {
            ++tcp_message_count;
        } else {
            ++udp_message_count;
        }
        callback_condition.notify_all();
    }
};

class EndpointServer : public Endpoint {
public:
    using Endpoint::Endpoint;

    void callback(bool close) override {
        if (close) {
            return;
        }

        std::shared_ptr<Socket> client_socket = std::static_pointer_cast<TCPServerSocket>(get_socket())->accept();

        std::shared_ptr<Endpoint> client_endpoint = std::make_shared<EndpointClient>(client_socket);
        client_endpoint->start_listen(mux);

        endpoint_list[client_socket->get_socket_fd()] = client_endpoint;
        {
            std::lock_guard<std::mutex> lock(callback_mutex);
            ++on_connect_count;
        }
        callback_condition.notify_all();
    }
};

void udp_run() {
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_ip_address("127.0.0.1");
    addr->set_reliable(false);
    addr->set_port_address(get_test_port());

    UDPSocket client(addr);

    client.send(message, sizeof(message), addr);
}

void tcp_run() {
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_ip_address("127.0.0.1");
    addr->set_reliable(true);
    addr->set_port_address(get_test_port());

    TCPClientSocket client(addr);

    client.send(client.get_socket_fd(), message, sizeof(message));

    std::unique_lock<std::mutex> lock(callback_mutex);
    callback_condition.wait_for(lock, std::chrono::seconds(1), [] { return tcp_message_count > 0; });
}
TEST(Multiplexer, Multiplexer) {
    std::shared_ptr<std::thread> udp_thread, tcp_thread;
    create_msg(message, kMaxLen);
    {
        std::lock_guard<std::mutex> lock(callback_mutex);
        on_message_count = 0;
        tcp_message_count = 0;
        udp_message_count = 0;
        on_connect_count = 0;
        messages_match = true;
    }
    mux->start();

    // Add TCP Server Socket to Multiplexer
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_ip_address("127.0.0.1");
    addr->set_reliable(true);
    addr->set_port_address(get_test_port());

    std::shared_ptr<TCPServerSocket> server = std::make_shared<TCPServerSocket>(addr);
    ASSERT_EQ(0, server->listen());

    EndpointServer ep(server);
    ep.start_listen(mux);

    tcp_thread = std::make_shared<std::thread>(tcp_run);

    // Add UDP Socket to Multiplexer
    std::shared_ptr<Address> addr2 = std::make_shared<IP4Address>();
    addr2->set_ip_address("127.0.0.1");
    addr2->set_reliable(false);
    addr2->set_port_address(get_test_port());

    std::shared_ptr<UDPSocket> server2 = std::make_shared<UDPSocket>(addr2);
    server2->bind();

    EndpointClient ep2(server2);
    ep2.start_listen(mux);

    udp_thread = std::make_shared<std::thread>(udp_run);

    std::unique_lock<std::mutex> lock(callback_mutex);
    const bool received_messages = callback_condition.wait_for(
        lock, std::chrono::seconds(2), [] { return on_message_count == 2 && on_connect_count == 1; });
    lock.unlock();

    udp_thread->join();
    tcp_thread->join();
    mux->stop();
    mux->join();
    endpoint_list.clear();

    ASSERT_TRUE(received_messages);
    ASSERT_TRUE(messages_match);
    ASSERT_EQ(tcp_message_count, 1);
    ASSERT_EQ(udp_message_count, 1);
    ASSERT_EQ(on_connect_count, 1);
}
