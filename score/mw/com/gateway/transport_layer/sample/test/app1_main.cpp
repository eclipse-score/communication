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
#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport.h"
#include "score/mw/com/gateway/transport_layer/sample/configuration/hypervisor_socket_configuration.h"

#include <atomic>
#include <iostream>
#include <string>
#include <thread>

using score::mw::com::gateway::BidirectionalTransport;
using score::mw::com::gateway::HyperVisorSocketConfiguration;

namespace
{

HyperVisorSocketConfiguration CreateConfiguration()
{
    HyperVisorSocketConfiguration config{};
    config.remote_ip_ = score::os::Ipv4Address{"127.0.0.1"};
    config.local_port_ = 9001;
    config.remote_port_ = 9000;
    return config;
}

int ExecuteWithRegularConnection()
{
    // App app1 is expected to receive 2 messages from app2 and will in return send 1 message back to app2.
    int expected_amount_received_messages = 2;

    auto config = CreateConfiguration();

    BidirectionalTransport transport{std::move(config)};
    const auto setup_result = transport.Setup();
    if (!setup_result)
    {
        std::cerr << "Transport setup failed: " << setup_result.error() << std::endl;
        return 1;
    }

    int received_message_count = 0;

    transport.SetMessageHandler([&](std::unique_ptr<score::mw::com::gateway::TransportMessage> message) {
        std::cout << "Received message of type: " << static_cast<int>(message->GetHeader().type) << "\n";
        ++received_message_count;
        if (received_message_count == expected_amount_received_messages)
        {
            auto ack_response = score::mw::com::gateway::UpdateNotification{
                score::mw::com::impl::InstanceSpecifier::Create(std::string{"TestService/Instance1"}).value(),
                score::mw::com::impl::ServiceElementType::EVENT,
                "TestEvent"};
            transport.SendNotification(ack_response);
        }
    });

    int sleep_counter = 0;
    // Sleep until all expected messages are received or a timeout occurs (whichever comes first)
    while (received_message_count < expected_amount_received_messages || sleep_counter < 200)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sleep_counter++;
    }

    if (received_message_count != expected_amount_received_messages)
    {
        std::cerr << "Amount of expected messages have not been received. Received " << received_message_count
                  << " messages, expected " << expected_amount_received_messages << " messages." << std::endl;
        return 1;
    }

    return 0;
}

int ExecuteWithReconnect()
{
    // App1 will receive one message, will disconnect and after the reconnect receive one more message and respond with
    // a notification.
    constexpr int kMessagesBeforeDisconnect = 1;
    constexpr int kMessagesAfterReconnect = 1;
    constexpr int kExpectedSendMessages = 1;
    constexpr int kTotalExpectedMessages = kMessagesBeforeDisconnect + kMessagesAfterReconnect;

    auto config = CreateConfiguration();
    BidirectionalTransport transport{std::move(config)};
    const auto setup_result = transport.Setup();
    if (!setup_result)
    {
        std::cerr << "Reconnect: Transport setup failed: " << setup_result.error() << std::endl;
        return 1;
    }

    std::atomic<int> received_message_count{0};
    std::atomic<int> sent_message_count{0};
    std::atomic<bool> disconnect_detected{false};

    transport.SetMessageHandler([&](std::unique_ptr<score::mw::com::gateway::TransportMessage> message) {
        const int count = ++received_message_count;

        // Send a notification back to the sender when receiving the message after reconnect
        if (count == kTotalExpectedMessages)
        {
            auto notification = score::mw::com::gateway::UpdateNotification{
                score::mw::com::impl::InstanceSpecifier::Create(std::string{"TestService/Instance1"}).value(),
                score::mw::com::impl::ServiceElementType::EVENT,
                "TestEvent"};
            transport.SendNotification(notification);
            sent_message_count++;
        }
    });

    // Wait for the first message before disconnect
    constexpr int kTimeoutMs = 15000;
    constexpr int kSleepIntervalMs = 50;
    int elapsed = 0;
    while (received_message_count < kMessagesBeforeDisconnect && elapsed < kTimeoutMs)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSleepIntervalMs));
        elapsed += kSleepIntervalMs;
    }
    if (received_message_count < kMessagesBeforeDisconnect)
    {
        std::cerr << "App1: Timed out waiting for messages before disconnect. Received "
                  << received_message_count.load() << std::endl;
        return 1;
    }
    std::cout << "App1: Received pre-disconnect messages, waiting for disconnect and reconnect..." << std::endl;

    // Wait for disconnect to be detected (App2 has been shut down)
    elapsed = 0;
    while (transport.IsConnected() && elapsed < kTimeoutMs)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSleepIntervalMs));
        elapsed += kSleepIntervalMs;
    }
    if (transport.IsConnected())
    {
        std::cout << "App1: Warning - No disconnected detected, but might have reconnected to quickly." << std::endl;
    }
    else
    {
        std::cout << "App1: Disconnect detected, waiting for reconnect" << std::endl;
    }

    // Wait for reconnect
    elapsed = 0;
    while (!transport.IsConnected() && elapsed < kTimeoutMs)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSleepIntervalMs));
        elapsed += kSleepIntervalMs;
    }
    if (!transport.IsConnected())
    {
        std::cerr << "App1: Timed out waiting for reconnect." << std::endl;
        return 1;
    }

    // Wait for the remaining message after reconnect
    elapsed = 0;
    while (received_message_count < kTotalExpectedMessages && elapsed < kTimeoutMs)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSleepIntervalMs));
        elapsed += kSleepIntervalMs;
    }

    if (received_message_count != kTotalExpectedMessages)
    {
        std::cerr << "App1: Expected " << kTotalExpectedMessages << " total messages but received "
                  << received_message_count.load() << std::endl;
        return 1;
    }

    // Wait for some time to ensure that the notification has been sent (so that app2 can terminate correctly)
    elapsed = 0;
    while (sent_message_count < kExpectedSendMessages && elapsed < kTimeoutMs)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSleepIntervalMs));
        elapsed += kSleepIntervalMs;
    }

    std::cout << "App1: All messages received successfully after reconnect and response sent." << std::endl;
    return 0;
}

}  // namespace

int main(int argc, char* argv[])
{
    std::string mode = "regular_case";
    if (argc > 1)
    {
        mode = argv[1];
    }

    if (mode == "reconnect")
    {
        return ExecuteWithReconnect();
    }
    return ExecuteWithRegularConnection();
}
