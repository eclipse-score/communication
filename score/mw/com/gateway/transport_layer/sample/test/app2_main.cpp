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

#include <iostream>
#include <memory>
#include <thread>

using score::mw::com::gateway::BidirectionalTransport;
using score::mw::com::gateway::HyperVisorSocketConfiguration;

HyperVisorSocketConfiguration CreateConfiguration()
{
    HyperVisorSocketConfiguration config{};
    config.remote_ip_ = score::os::Ipv4Address{"127.0.0.1"};
    config.local_port_ = 9000;
    config.remote_port_ = 9001;
    return config;
}

void SendMessages(BidirectionalTransport& transport)
{
    auto service_request = score::mw::com::gateway::ProvideServiceRequest{
        score::mw::com::impl::InstanceSpecifier::Create("TestService/Instance1").value(),
        {{"TestEvent", {64U, 8U}}, {"TestField", {32U, 4U}}},
        4U,
        8U};
    const auto send_result = transport.SendRequest(service_request);

    auto notification = score::mw::com::gateway::RegisterNotificationRequest{
        score::mw::com::impl::InstanceSpecifier::Create("TestService/Instance1").value(),
        score::mw::com::impl::ServiceElementType::EVENT,
        "TestEvent"};
    const auto notification_result = transport.SendNotification(notification);
}

int main()
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
    while (received_message_count < expected_amount_received_messages || sleep_counter < 100)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
