/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#include "gtest/gtest.h"
#include "score/socom/clients_t.hpp"
#include "score/socom/server_t.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/single_connection_test_fixture.hpp"
#include "score/socom/utilities.hpp"
#include "score/socom/vector_payload.hpp"

namespace score::socom {

using Conf_instance = std::pair<Server_service_interface_definition, Service_instance>;

using MultiConnectionTest = SingleConnectionTest;

TEST_F(MultiConnectionTest, ClientAndServerInDifferentThreadsCommunicateRaceFree) {
    auto const mr = Method_result{Application_return(clone_payload(real_payload))};
    auto const num_method_calls = 100;
    auto const num_events = 1000;

    Server_data server{connector_factory};
    server.expect_event_subscription(event_id);

    Client_data client{connector_factory};
    auto const sub0 = client.create_event_subscription(event_id);
    // Google Mock does not allow EXPECT_CALL statements concurrently from multiple threads
    server.expect_and_respond_method_calls(num_method_calls, method_id, empty_payload(), mr);
    auto const& events_received = client.expect_event_updates(num_events, event_id, real_payload);

    auto const server_thread = [this, &server]() {
        for (auto i = 0; i < num_events; i++) {
            server.update_event(event_id, real_payload);
        }
    };

    auto const client_thread = [&mr, &client]() {
        wait_for_atomics(
            client.expect_and_call_methods(num_method_calls, method_id, empty_payload(), mr));
    };

    auto const server_return = std::async(std::launch::async, server_thread);
    auto const client_return = std::async(std::launch::async, client_thread);
    wait_for_atomics(events_received);
}

TEST_F(MultiConnectionTest, ClientAndServerCreatedInDifferentThreadsCommunicateRaceFree) {
    auto const mr = Method_result{Application_return(clone_payload(real_payload))};
    auto const num_method_calls = 100;
    auto const num_events = 1000;

    std::atomic<bool> client_done{false};
    // Google Mock does not allow EXPECT_CALL statements concurrently from multiple threads
    std::atomic<bool> client_ready{false};
    std::atomic<bool> server_ready{false};

    auto const server_thread = [this, &mr, &server_ready, &client_ready, &client_done]() {
        Server_data server{connector_factory};
        server.expect_event_subscription(event_id);

        server.expect_and_respond_method_calls(num_method_calls, method_id, empty_payload(), mr);
        server_ready = true;
        wait_for_atomics(client_ready);
        for (auto i = 0; i < num_events; i++) {
            server.update_event(event_id, real_payload);
        }
        wait_for_atomics(client_done);
    };

    auto const client_thread = [this, &mr, &server_ready, &client_ready, &client_done]() {
        {  // Destroy client before server to prevent call of on_service_state_change callback
            wait_for_atomics(server_ready);
            Client_data client{connector_factory};
            auto const sub0 = client.create_event_subscription(event_id);
            auto const& events_received =
                client.expect_event_updates(num_events, event_id, real_payload);
            client_ready = true;
            wait_for_atomics(
                client.expect_and_call_methods(num_method_calls, method_id, empty_payload(), mr));
            wait_for_atomics(events_received);
        }
        client_done = true;
    };

    auto const server_return = std::async(std::launch::async, server_thread);
    auto const client_return = std::async(std::launch::async, client_thread);
}

}  // namespace score::socom
