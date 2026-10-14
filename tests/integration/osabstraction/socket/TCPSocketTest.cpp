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
#include <sys/socket.h>
#include <atomic>
#include <chrono>
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

namespace {

void set_socket_buffers(int fd, int size) {
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
}

// Connects a client to a server whose small socket buffers fill up quickly.
struct BackpressureConnection {
    std::unique_ptr<TCPServerSocket> server;
    std::unique_ptr<TCPClientSocket> client;
    std::shared_ptr<Socket> accepted;
};

void connect_with_small_buffers(BackpressureConnection& connection) {
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_ip_address("127.0.0.1");
    addr->set_reliable(true);
    addr->set_port_address(get_test_port());

    connection.server = std::make_unique<TCPServerSocket>(addr);
    ASSERT_NE(connection.server->get_socket_fd(), -1);
    // Accepted sockets inherit the receive buffer size of the listening socket.
    set_socket_buffers(connection.server->get_socket_fd(), 32 * 1024);
    ASSERT_EQ(0, connection.server->listen());

    connection.client = std::make_unique<TCPClientSocket>(addr);
    set_socket_buffers(connection.client->get_socket_fd(), 32 * 1024);
    connection.accepted = connection.server->accept();
    ASSERT_NE(connection.accepted->get_socket_fd(), -1);

}

} // namespace

TEST(TCPSocketTest, SendWaitsForSlowReaderAcrossMultipleTimeouts) {
    BackpressureConnection connection;
    connect_with_small_buffers(connection);
    ASSERT_NE(connection.client, nullptr);

    // At about 3 MB/s the reader needs well over the 1 s send timeout, but it never stalls.
    constexpr std::size_t kMessageSize = 6 * 1024 * 1024;
    std::vector<char> message(kMessageSize, 'x');
    std::atomic<std::size_t> received{0};
    std::atomic<bool> send_failed{false};
    std::thread reader([&] {
        std::vector<char> buffer(16 * 1024);
        while (received < kMessageSize && !send_failed) {
            int length = connection.accepted->receive(buffer.data(), buffer.size());
            if (length < 0) {
                break;
            }
            received += static_cast<std::size_t>(length);
            usleep(5000);
        }
    });

    auto start = std::chrono::steady_clock::now();
    int sent = connection.client->send(connection.client->get_socket_fd(), message.data(), message.size());
    auto elapsed = std::chrono::steady_clock::now() - start;
    // A failed send leaves the reader at end-of-stream; stop it, but let it drain everything after a success.
    send_failed = (sent != static_cast<int>(kMessageSize));
    reader.join();

    EXPECT_EQ(sent, static_cast<int>(kMessageSize));
    EXPECT_EQ(received, kMessageSize);
    EXPECT_GT(elapsed, std::chrono::milliseconds(1200));
}

TEST(TCPSocketTest, SendGivesUpWhenReaderStalls) {
    BackpressureConnection connection;
    connect_with_small_buffers(connection);
    ASSERT_NE(connection.client, nullptr);

    std::vector<char> message(8 * 1024 * 1024, 'x');

    auto start = std::chrono::steady_clock::now();
    int sent = connection.client->send(connection.client->get_socket_fd(), message.data(), message.size());
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(sent, -1);
    EXPECT_GE(elapsed, std::chrono::milliseconds(900));
    EXPECT_LT(elapsed, std::chrono::seconds(5));
}
