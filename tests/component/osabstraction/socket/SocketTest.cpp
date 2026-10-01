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
#include <socket/Socket.h>

using namespace lgsomeip::osabstraction;

class SocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        address_ = std::make_shared<IP4Address>();
        address_->set_reliable(true);
    }
    std::shared_ptr<Address> address_;
};

TEST_F(SocketTest, get_socket_fd) {
    int fd = ::socket(PF_INET, SOCK_STREAM, 0);
    Socket socket(fd, address_);
    ASSERT_EQ(fd, socket.get_socket_fd());
}

TEST_F(SocketTest, constructor) {
    Socket socket(address_);
    ASSERT_NE(socket.get_socket_fd(), -1);

    int second_fd = ::socket(PF_INET, SOCK_STREAM, 0);
    ASSERT_NE(second_fd, -1);
    Socket socket2(second_fd, address_);
    ASSERT_NE(socket.get_socket_fd(), socket2.get_socket_fd());
}

TEST_F(SocketTest, deconstructor) {
    int fd = -1;
    {
        Socket socket(address_);
        fd = socket.get_socket_fd();
    }

    ASSERT_EQ(::close(fd), -1);
}

TEST_F(SocketTest, close_socket) {
    Socket socket(address_);
    socket.close_socket();
    ASSERT_EQ(socket.get_socket_fd(), kInvalidSocket);
}

TEST_F(SocketTest, create_socket) {
    Socket socket(kInvalidSocket, address_);
    ASSERT_EQ(socket.get_socket_fd(), kInvalidSocket);

    socket.create_socket();
    ASSERT_NE(socket.get_socket_fd(), kInvalidSocket);
}
