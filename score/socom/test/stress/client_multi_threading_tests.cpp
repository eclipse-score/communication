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

#include <cstddef>
#include <future>

#include "gtest/gtest.h"
#include "score/socom/clients_t.hpp"
#include "score/socom/multi_threaded_test_template.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/server_t.hpp"
#include "score/socom/single_connection_test_fixture.hpp"

using namespace std::chrono_literals;

namespace score::socom {

bool always_successful() { return true; }

auto is_ready(std::future<void> const& future) {
    return [&future]() { return std::future_status::ready == future.wait_for(0ms); };
}

class ClientMultiThreadingTest : public SingleConnectionTest {
   protected:
    Server_data server{connector_factory};
    Client_data client{connector_factory};
    std::size_t const minimum_num_event_updates = 100;
    std::size_t const min_method_calls = 100;
};

TEST_F(ClientMultiThreadingTest, SubscribeEventIsCalledByTwoThreadsWithoutRaceConditions) {
    server.expect_event_subscription(event_id);

    auto const events_received = client.expect_event_updates_min_number(minimum_num_event_updates,
                                                                        event_id, empty_payload());

    auto const subscribe_event = [this]() { client.subscribe_event(event_id); };
    auto const update_event = [this]() { server.update_event(event_id, empty_payload()); };

    multi_threaded_test_template({update_event, subscribe_event, subscribe_event},
                                 is_ready(events_received));

    // Make sure that server.expect_event_subscription(event_id) is always fulfilled. When running
    // with valgrind it happens very rarely that the client is able to subscribe.
    client.subscribe_event(event_id);
}

TEST_F(ClientMultiThreadingTest, UnsubscribeEventIsCalledByTwoThreadsWithoutRaceConditions) {
    auto const unsubscribe_event = [this]() { client.unsubscribe_event(event_id); };

    multi_threaded_test_template({unsubscribe_event, unsubscribe_event}, always_successful);
}

TEST_F(ClientMultiThreadingTest, RequestEventUpdateIsCalledByTwoThreadsWithoutRaceConditions) {
    server.expect_event_subscription(event_id);
    client.subscribe_event(event_id);
    server.expect_update_event_requests(event_id);

    auto const events_received = client.expect_requested_event_updates_min_number(
        minimum_num_event_updates, event_id, empty_payload());

    auto const request_event_update = [this]() { client.request_event_update(event_id); };
    auto const update_event = [this]() {
        server.update_requested_event(event_id, empty_payload());
    };

    multi_threaded_test_template({update_event, request_event_update, request_event_update},
                                 is_ready(events_received));

    // Make sure that server.expect_update_event_request(event_id) is always fulfilled. When running
    // with valgrind it happens very rarely that the client is able to subscribe.
    client.request_event_update(event_id);
}

TEST_F(ClientMultiThreadingTest, CallMethodIsCalledByTwoThreadsWithoutRaceConditions) {
    auto const method_called =
        server.expect_method_calls(min_method_calls, method_id, empty_payload());

    auto const call_method = [this]() {
        (void)client.call_method_fire_and_forget_and_return_invocation(method_id, empty_payload());
    };

    multi_threaded_test_template({call_method, call_method}, is_ready(method_called));
}

TEST_F(ClientMultiThreadingTest, GetPeerCredentialsIsCalledByTwoThreadsWithoutRaceConditions) {
    auto const get_peer_credentials = [this]() { (void)client.get_peer_credentials(); };

    multi_threaded_test_template({get_peer_credentials, get_peer_credentials}, always_successful);
}

}  // namespace score::socom
