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
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

using score::mw::com::gateway::BidirectionalTransport;
using score::mw::com::gateway::HyperVisorSocketConfiguration;

namespace
{

constexpr std::uint16_t kApp2LocalPort{9000U};
constexpr std::uint16_t kApp2RemotePort{9001U};
constexpr std::uint32_t kTestEventPayloadSize{64U};
constexpr std::uint32_t kTestEventPayloadAlignment{8U};
constexpr std::uint32_t kTestFieldPayloadSize{32U};
constexpr std::uint32_t kSharedMemoryControlSize{4U};
constexpr std::uint32_t kSharedMemoryDataSize{8U};
constexpr int kRegularConnectionTimeoutPollCount{100};
constexpr std::chrono::milliseconds kRegularConnectionPollInterval{50};

HyperVisorSocketConfiguration CreateConfiguration()
{
    HyperVisorSocketConfiguration config{};
    config.remote_ip_ = score::os::Ipv4Address{"127.0.0.1"};
    config.local_port_ = kApp2LocalPort;
    config.remote_port_ = kApp2RemotePort;
    return config;
}

void SendMessages(BidirectionalTransport& transport)
{
    auto service_request = score::mw::com::gateway::ProvideServiceRequest{
        score::mw::com::impl::InstanceSpecifier::Create(std::string{"TestService/Instance1"}).value(),
        {{"TestEvent", {kTestEventPayloadSize, kTestEventPayloadAlignment}},
         {"TestField", {kTestFieldPayloadSize, kSharedMemoryControlSize}}},
        kSharedMemoryControlSize,
        kSharedMemoryDataSize};
    const auto send_result = transport.SendRequest(service_request);
    static_cast<void>(send_result);

    auto notification = score::mw::com::gateway::RegisterNotificationRequest{
        score::mw::com::impl::InstanceSpecifier::Create(std::string{"TestService/Instance1"}).value(),
        score::mw::com::impl::ServiceElementType::EVENT,
        "TestEvent"};
    const auto notification_result = transport.SendNotification(notification);
    if (!notification_result)
    {
        std::cerr << "Failed to send notification: " << notification_result.error() << std::endl;
    }
}

int ExecuteWithRegularConnection()
{
    // App app2 is expected to send 2 messages to receiver and will in return receive 1 message back from app1.
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
        std::cout << "Sender App: Received message of type " << static_cast<int>(message->GetHeader().type)
                  << std::endl;
        received_message_count++;
    });

    SendMessages(transport);

    int expected_amount_received_messages = 1;
    int sleep_counter = 0;
    // Sleep until all expected messages are received or a timeout occurs (whichever comes first)
    while (received_message_count < expected_amount_received_messages ||
           sleep_counter < kRegularConnectionTimeoutPollCount)
    {
        std::this_thread::sleep_for(kRegularConnectionPollInterval);
        sleep_counter++;
    }

    if (expected_amount_received_messages != received_message_count)
    {
        std::cerr << "Amount of expected messages have not been received. Received " << received_message_count
                  << " messages, expected " << expected_amount_received_messages << " messages." << std::endl;
        return 1;
    }
    return 0;
}

int ExecuteWithReconnect()
{
    // App2 will shut down after sending first message, reconnect and send one mor message and wait for a return
    // notification from app1.

    constexpr int kSleepIntervalMs = 50;
    constexpr int kTimeoutMs = 15000;

    {
        auto config = CreateConfiguration();
        BidirectionalTransport transport{std::move(config)};
        const auto setup_result = transport.Setup();
        if (!setup_result)
        {
            std::cerr << "App2: Transport setup failed: " << setup_result.error() << std::endl;
            return 1;
        }

        auto request = score::mw::com::gateway::ProvideServiceRequest{
            score::mw::com::impl::InstanceSpecifier::Create(std::string{"TestService/Instance1"}).value(),
            {{"TestEvent", {kTestEventPayloadSize, kTestEventPayloadAlignment}}},
            kSharedMemoryControlSize,
            kSharedMemoryDataSize};
        const auto send_result = transport.SendRequest(request);
        if (!send_result)
        {
            std::cerr << "App2: SendRequest failed: " << send_result.error() << std::endl;
            return 1;
        }

        std::cout << "App2: Message sent, shutting down to trigger disconnect" << std::endl;
        transport.Shutdown();
    }

    // Pause in order to let app1 handle the disconnect
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Reconnect and send message
    {
        auto config = CreateConfiguration();
        BidirectionalTransport transport{std::move(config)};
        const auto setup_result = transport.Setup();
        if (!setup_result)
        {
            std::cerr << "App2: Transport setup failed when trying to reconnect: " << setup_result.error() << std::endl;
            return 1;
        }

        std::atomic<int> received_message_count{0};
        transport.SetMessageHandler([&](std::unique_ptr<score::mw::com::gateway::TransportMessage> message) {
            std::cout << "App2: Received message of type " << static_cast<int>(message->GetHeader().type) << std::endl;
            received_message_count++;
        });

        auto request = score::mw::com::gateway::ProvideServiceRequest{
            score::mw::com::impl::InstanceSpecifier::Create(std::string{"TestService/Instance1"}).value(),
            {{"TestField", {kTestFieldPayloadSize, kSharedMemoryControlSize}}},
            kSharedMemoryControlSize,
            kSharedMemoryDataSize};
        const auto send_result = transport.SendRequest(request);
        if (!send_result)
        {
            std::cerr << "App2: SendRequest failed: " << send_result.error() << std::endl;
            return 1;
        }

        std::cout << "App2: Waiting for notification response from App1." << std::endl;

        // Wait for response notification from app1

        int expected_amount_received_messages = 1;
        int elapsed = 0;
        while (received_message_count < expected_amount_received_messages && elapsed < kTimeoutMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(kSleepIntervalMs));
            elapsed += kSleepIntervalMs;
        }

        if (received_message_count != expected_amount_received_messages)
        {
            std::cerr << "App2: Expected " << expected_amount_received_messages << " messages but received "
                      << received_message_count.load() << std::endl;
            return 1;
        }

        std::cout << "App2: Finished successfully" << std::endl;
    }

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
