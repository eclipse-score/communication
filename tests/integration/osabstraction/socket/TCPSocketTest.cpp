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

#include <gtest/gtest.h>
#include <socket/IP4Address.h>
#include <socket/IP6Address.h>
#include <socket/LocalAddress.h>
#include <socket/TCPClientSocket.h>
#include <socket/TCPServerSocket.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "Helper.h"

using namespace lgsomeip::osabstraction;

constexpr int kTcpMax = 5000;
char tcp_message[kTcpMax];
char tcp_client_received[kTcpMax];
int tcp_client_received_length = 0;
std::string local_socket_path;

class ScopedUnixSocketFiles {
public:
    explicit ScopedUnixSocketFiles(const char* path) : path_(path) {}

    ~ScopedUnixSocketFiles() {
        std::remove(path_.c_str());
        std::remove((path_ + ".lock").c_str());
    }

private:
    std::string path_;
};

int receive_until_full(Socket& socket, char* buffer, std::size_t size) {
    std::size_t received_length = 0;
    for (int attempt = 0; attempt < 100 && received_length < size; ++attempt) {
        int current_length = socket.receive(buffer + received_length, size - received_length);
        if (current_length < 0) {
            return current_length;
        }
        received_length += static_cast<std::size_t>(current_length);
        if (received_length < size) {
            usleep(1000);
        }
    }
    return static_cast<int>(received_length);
}

void tcp_client_run() {
    usleep(10);
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_ip_address("127.0.0.1");
    addr->set_reliable(true);
    addr->set_port_address(get_test_port());

    TCPClientSocket client(addr);

    client.send(client.get_socket_fd(), tcp_message, sizeof(tcp_message));
    tcp_client_received_length = receive_until_full(client, tcp_client_received, sizeof(tcp_client_received));
}
TEST(TCPSocketTest, TCPSocketTest) {
    char message[kTcpMax];
    create_msg(tcp_message, kTcpMax);

    std::shared_ptr<std::thread> thread;

    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_reliable(true);
    addr->set_port_address(get_test_port());

    TCPServerSocket server(addr); // create_socket
    ASSERT_NE(server.get_socket_fd(), -1);

    ASSERT_EQ(0, server.listen()); // listen
    thread = std::make_shared<std::thread>(tcp_client_run);

    std::shared_ptr<Socket> client_socket = server.accept();
    ASSERT_NE(client_socket->get_socket_fd(), -1); // accept

    int received_length = receive_until_full(*client_socket, message, sizeof(message));
    ASSERT_EQ(received_length, sizeof(message));
    ASSERT_STREQ(tcp_message, message); // send & receive
    client_socket->send(client_socket->get_socket_fd(), tcp_message, sizeof(tcp_message));
    thread->join();
    ASSERT_EQ(tcp_client_received_length, sizeof(tcp_client_received));
    ASSERT_STREQ(tcp_message, tcp_client_received);
}
void tcp_client_ipv6_run() {
    usleep(10);
    std::shared_ptr<Address> addr = std::make_shared<IP6Address>();
    addr->set_ip_address("::1");
    addr->set_reliable(true);
    addr->set_port_address(get_test_port());

    TCPClientSocket client(addr);

    client.send(client.get_socket_fd(), tcp_message, sizeof(tcp_message));
    tcp_client_received_length = receive_until_full(client, tcp_client_received, sizeof(tcp_client_received));
}
TEST(TCPSocketTest, TCPSocketIPv6Test) {
    char message[kTcpMax];
    create_msg(tcp_message, kTcpMax);

    std::shared_ptr<std::thread> thread;

    std::shared_ptr<Address> addr = std::make_shared<IP6Address>();
    addr->set_reliable(true);
    addr->set_port_address(get_test_port());

    TCPServerSocket server(addr); // create_socket
    ASSERT_NE(server.get_socket_fd(), -1);

    ASSERT_EQ(0, server.listen()); // listen
    thread = std::make_shared<std::thread>(tcp_client_ipv6_run);

    std::shared_ptr<Socket> client_socket = server.accept();
    ASSERT_NE(client_socket->get_socket_fd(), -1); // accept

    int received_length = receive_until_full(*client_socket, message, sizeof(message));
    ASSERT_EQ(received_length, sizeof(message));
    ASSERT_STREQ(tcp_message, message); // send & receive
    client_socket->send(client_socket->get_socket_fd(), tcp_message, sizeof(tcp_message));
    thread->join();
    ASSERT_EQ(tcp_client_received_length, sizeof(tcp_client_received));
    ASSERT_STREQ(tcp_message, tcp_client_received);
}
constexpr int kLocalMax = 5000;
char local_message[kLocalMax];

void local_tcp_client_run() {
    usleep(10);

    std::shared_ptr<Address> addr = std::make_shared<LocalAddress>();
    addr->set_reliable(true);
    addr->set_file_path(local_socket_path);

    TCPClientSocket client(addr);

    client.send(client.get_socket_fd(), local_message, sizeof(local_message));
    tcp_client_received_length = receive_until_full(client, tcp_client_received, sizeof(tcp_client_received));
}
TEST(TCPSocketTest, LocalTCPSocket) {
    char message[kLocalMax];
    local_socket_path = get_test_socket_path("LocalSocketTest");
    ScopedUnixSocketFiles socket_files(local_socket_path.c_str());
    std::shared_ptr<std::thread> thread;
    create_msg(local_message, kLocalMax);

    std::shared_ptr<Address> addr = std::make_shared<LocalAddress>();
    addr->set_reliable(true);
    addr->set_file_path(local_socket_path);

    TCPServerSocket server(addr); // create_socket
    ASSERT_NE(server.get_socket_fd(), -1);

    ASSERT_EQ(0, server.listen()); // listen
    thread = std::make_shared<std::thread>(local_tcp_client_run);

    std::shared_ptr<Socket> client_socket = server.accept();
    ASSERT_NE(client_socket->get_socket_fd(), -1); // accept

    int received_length = receive_until_full(*client_socket, message, sizeof(message));
    ASSERT_EQ(received_length, sizeof(message));
    ASSERT_STREQ(local_message, message); // send & receive

    client_socket->send(client_socket->get_socket_fd(), local_message, sizeof(local_message));
    thread->join();
    ASSERT_EQ(tcp_client_received_length, sizeof(tcp_client_received));
    ASSERT_STREQ(local_message, tcp_client_received);
}
