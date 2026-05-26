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
    config.local_port_ = 9001;
    config.remote_port_ = 9000;
    return config;
}

int main()
{
    // App app1 is expected to receive 2 messages from sender and will in return send 1 message back to app2.
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
                score::mw::com::impl::InstanceSpecifier::Create("TestService/Instance1").value(),
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
