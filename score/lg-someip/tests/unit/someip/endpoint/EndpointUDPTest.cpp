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
 *
 ********************************************************************************/

#include <gtest/gtest.h>

#include <unistd.h>

#include <utils/log/logger.h>

#include <endpoint/EndpointUDP.h>
#include <message/MessageSOMEIP.h>
#include <socket/IP4Address.h>
#include <socket/UDPSocket.h>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

namespace {

struct EndpointUDPTestHost {
    void on_message(std::shared_ptr<Endpoint>, std::uint8_t* message, std::size_t message_length) {
        received.emplace_back(message, message + message_length);
    }

    std::vector<std::vector<std::uint8_t>> received;
};

std::vector<std::uint8_t> make_message(std::size_t payload_size, std::uint8_t fill) {
    MessageSOMEIP message;
    message.set_message_id(0x10010001);
    message.set_message_type(SOMEIP_MESSAGE_TYPE::REQUEST);

    std::vector<std::uint8_t> payload(payload_size, fill);
    if (payload_size > 0) {
        message.set_payload(payload.data(), static_cast<std::uint32_t>(payload_size));
    }

    std::vector<std::uint8_t> bytes(message.get_serialized_size());
    message.serialize(bytes.data());
    return bytes;
}

class EndpointUDPTest : public ::testing::Test {
protected:
    void SetUp() override {
        address_ = std::make_shared<IP4Address>();
        address_->set_ip_address("127.0.0.1");
        address_->set_reliable(false);
        address_->set_port_address(static_cast<std::uint16_t>(40000 + getpid() % 10000));

        auto receiver = std::make_shared<UDPSocket>(address_);
        receiver->bind();

        endpoint_ = std::make_shared<EndpointUDP<EndpointUDPTestHost>>(&host_);
        endpoint_->set_socket(receiver);
    }

    std::vector<std::vector<std::uint8_t>> deliver(const std::vector<std::uint8_t>& datagram) {
        UDPSocket sender(address_);
        sender.send(datagram.data(), datagram.size(), address_);

        endpoint_->callback(false);
        return host_.received;
    }

    std::shared_ptr<IP4Address> address_;
    EndpointUDPTestHost host_;
    std::shared_ptr<EndpointUDP<EndpointUDPTestHost>> endpoint_;
};

} // namespace

TEST_F(EndpointUDPTest, receives_message_with_maximum_payload) {
    const auto datagram = make_message(SOMEIP_UDP::MAX_PAYLOAD_SIZE, 0x5A);
    ASSERT_EQ(datagram.size(), SOMEIP_UDP::MAX_MESSAGE_SIZE);

    const auto received = deliver(datagram);

    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received[0], datagram);
}

TEST_F(EndpointUDPTest, discards_message_with_payload_over_limit) {
    const auto received = deliver(make_message(SOMEIP_UDP::MAX_PAYLOAD_SIZE + 1, 0x5A));

    EXPECT_TRUE(received.empty());
}

TEST_F(EndpointUDPTest, splits_datagram_larger_than_one_message_limit) {
    const auto first = make_message(SOMEIP_UDP::MAX_MESSAGE_SIZE / 2, 0x11);
    const auto second = make_message(SOMEIP_UDP::MAX_MESSAGE_SIZE / 2, 0x22);
    std::vector<std::uint8_t> datagram(first);
    datagram.insert(datagram.end(), second.begin(), second.end());
    ASSERT_GT(datagram.size(), SOMEIP_UDP::MAX_MESSAGE_SIZE);
    if (datagram.size() >= SOMEIP_UDP::RECEIVE_BUFFER_SIZE) {
        GTEST_SKIP() << "two messages do not fit one datagram at this payload limit";
    }

    const auto received = deliver(datagram);

    ASSERT_EQ(received.size(), 2U);
    EXPECT_EQ(received[0], first);
    EXPECT_EQ(received[1], second);
}

TEST_F(EndpointUDPTest, discards_datagram_that_fills_receive_buffer) {
    auto datagram = make_message(10, 0x33);
    datagram.resize(SOMEIP_UDP::RECEIVE_BUFFER_SIZE + 10, 0x00);

    const auto received = deliver(datagram);

    EXPECT_TRUE(received.empty());
}
