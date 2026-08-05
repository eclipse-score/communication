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

#include <string_view>

#include "gtest/gtest.h"
#include "score/socom/clients_t.hpp"
#include "score/socom/connector_factory.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/server_t.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/vector_payload.hpp"

namespace score::socom {

struct Client_server {
    Server_data server;
    Client_data client;

    explicit Client_server(Connector_factory& factory) : server{factory}, client{factory} {}
};

class MultipleRuntimesTest : public ::testing::Test {
   protected:
    static constexpr std::string_view test_service_id{"TestInterface1"};
    static constexpr std::string_view test_instance_id{"TestInstance1"};
    Connector_factory factory0 = Connector_factory{
        Service_interface_identifier{test_service_id, {1, 0}}, to_num_of_methods(2U),
        to_num_of_events(3U), Service_instance{test_instance_id}};

    Connector_factory factory1 = Connector_factory{
        Service_interface_identifier{test_service_id, {2, 3}}, to_num_of_methods(2U),
        to_num_of_events(3U), Service_instance{test_instance_id}};

    Payload const real_payload = make_vector_payload(make_vector_buffer(1U, 2U, 3U, 4U));
    Payload const more_payload = make_vector_payload(
        make_vector_buffer(1U, 2U, 3U, 4U, 5U, 3U, 2U, 4U, 3U, 2U, 4U, 2U, 4U, 5U, 5U, 3U));
};

constexpr std::string_view MultipleRuntimesTest::test_service_id;
constexpr std::string_view MultipleRuntimesTest::test_instance_id;

TEST_F(MultipleRuntimesTest, StartingTwoServers) {
    Server_data server0{factory0};
    Server_data server1{factory1};
}

TEST_F(MultipleRuntimesTest, StartingTwoClients) {
    Client_data client0{factory0, Client_data::no_connect};
    Client_data client1{factory1, Client_data::no_connect};
}

TEST_F(MultipleRuntimesTest, ConnectingTwoTimesServerAndClient) {
    Client_server pair0{factory0};
    Client_server pair1{factory1};
}

TEST_F(MultipleRuntimesTest, MultipleServerAndClientsAreCreatedAndLastPairExchangesAnEvent) {
    Client_server pair0{factory0};
    Client_server pair1{factory1};

    auto const event = static_cast<Event_id>(factory0.get_num_events() / 2);

    pair1.server.expect_event_subscription(event);
    auto const sub = pair1.client.create_event_subscription(event);

    pair1.client.expect_event_update(event, real_payload);
    pair1.server.update_event(event, real_payload);
}

TEST_F(MultipleRuntimesTest, MultipleServerAndClientsAreCreatedAndFirstPairExchangesAnEvent) {
    Client_server pair0{factory0};
    Client_server pair1{factory1};

    auto event = Event_id{1};

    pair0.server.expect_event_subscription(event);
    auto const sub = pair0.client.create_event_subscription(event);

    pair0.client.expect_event_update(event, real_payload);
    pair0.server.update_event(event, real_payload);
}

TEST_F(MultipleRuntimesTest, MultipleServerAndClientsAreSendingEvents) {
    Client_server pair0{factory0};
    Client_server pair1{factory1};

    auto const event0 = static_cast<Event_id>(factory0.get_num_events() / 2);

    pair0.server.expect_event_subscription(event0);
    auto const sub0 = pair0.client.create_event_subscription(event0);
    pair0.client.expect_event_update(event0, real_payload);
    pair0.server.update_event(event0, real_payload);

    auto event1 = Event_id{1};

    pair1.server.expect_event_subscription(event1);
    auto const sub1 = pair1.client.create_event_subscription(event1);
    pair1.client.expect_event_update(event1, real_payload);
    pair1.server.update_event(event1, real_payload);
}

TEST_F(MultipleRuntimesTest, MultipleServerAndClientsAreCreatedAndFirstPairHasAMethodCall) {
    auto const method0 = static_cast<Event_id>(factory0.get_num_methods() / 2);
    auto const result0 = Method_result{Application_return{clone_payload(real_payload)}};

    Client_server pair0{factory0};
    Client_server pair1{factory1};

    pair0.server.expect_and_respond_method_calls(1, method0, real_payload, result0);
    pair0.client.expect_and_call_method(method0, real_payload, result0);
}

TEST_F(MultipleRuntimesTest, MultipleServerAndClientsAreCreatedAndSecondPairHasAMethodCall) {
    auto const method1 = Method_id{1};
    auto const result1 = Method_result{Application_return{clone_payload(real_payload)}};

    Client_server pair0{factory0};
    Client_server pair1{factory1};

    pair1.server.expect_and_respond_method_calls(1, method1, real_payload, result1);
    pair1.client.expect_and_call_method(method1, real_payload, result1);
}

TEST_F(MultipleRuntimesTest, MultipleServerAndClientsAndBothHaveMethodCalls) {
    auto const method0 = static_cast<Event_id>(factory0.get_num_methods() / 2);
    auto const result0 = Method_result{Application_return{clone_payload(real_payload)}};

    auto const method1 = Method_id{1};
    auto const result1 = Method_result{Application_return{clone_payload(more_payload)}};

    Client_server pair0{factory0};
    Client_server pair1{factory1};

    pair0.server.expect_and_respond_method_calls(1, method0, real_payload, result0);
    pair0.client.expect_and_call_method(method0, real_payload, result0);

    pair1.server.expect_and_respond_method_calls(1, method1, real_payload, result1);
    pair1.client.expect_and_call_method(method1, real_payload, result1);
}

TEST_F(MultipleRuntimesTest, BothRuntimesHaveSameInstanceIdAndConfigurationButAreStillSeparated) {
    auto const method0 = static_cast<Event_id>(factory0.get_num_methods() / 2);
    auto const result0 = Method_result{Application_return{clone_payload(real_payload)}};
    auto const result1 = Method_result{Application_return{clone_payload(more_payload)}};

    auto rt1 = Connector_factory{factory0};
    Client_server pair0{factory0};
    Client_server pair1{rt1};

    pair0.server.expect_and_respond_method_calls(1, method0, real_payload, result0);
    pair0.client.expect_and_call_method(method0, real_payload, result0);

    pair1.server.expect_and_respond_method_calls(1, method0, real_payload, result1);
    pair1.client.expect_and_call_method(method0, real_payload, result1);
}

}  // namespace score::socom
