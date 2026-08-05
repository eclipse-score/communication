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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "score/socom/callback_mocks.hpp"
#include "score/socom/runtime.hpp"

using ::testing::_;

namespace score::socom {

class Runtime_test : public ::testing::Test {
   protected:
    Server_service_interface_definition config{
        Service_interface_identifier{"example.interface", Literal_tag{}, {1, 0}},
        to_num_of_methods(1), to_num_of_events(1)};
    Service_instance instance{"instance1", Literal_tag{}};

    Runtime::Uptr runtime = create_runtime();

    // Client_connector mocks
    Service_state_change_callback_mock m_service_state_change_mock;
    Event_update_callback_mock m_event_update_mock;
    Event_update_callback_mock m_requested_event_update_mock;
    Event_payload_allocate_callback_mock m_event_payload_allocate_mock;

    // Server_connector mocks
    Event_subscription_change_callback_mock m_event_subscription_change_mock;
    Event_request_update_callback_mock m_event_update_request_mock;
    Method_call_credentials_callback_mock m_method_call_mock;
    Method_call_payload_allocate_callback_mock m_method_payload_allocate_mock;

    Disabled_server_connector::Callbacks make_server_callbacks() {
        return Disabled_server_connector::Callbacks{m_method_call_mock.as_function(),
                                                    m_event_subscription_change_mock.as_function(),
                                                    m_event_update_request_mock.as_function(),
                                                    m_method_payload_allocate_mock.as_function()};
    }

    Client_connector::Callbacks make_client_callbacks() {
        return Client_connector::Callbacks{m_service_state_change_mock.as_function(),
                                           m_event_update_mock.as_function(),
                                           m_requested_event_update_mock.as_function(),
                                           m_event_payload_allocate_mock.as_function()};
    }
};

class Connection_test : public Runtime_test {
   protected:
    void SetUp() override {
        Runtime_test::SetUp();

        auto client_connector_result =
            runtime->make_client_connector(config, instance, make_client_callbacks());
        auto server_connector_result =
            runtime->make_server_connector(config, instance, make_server_callbacks());

        ASSERT_TRUE(client_connector_result);
        ASSERT_TRUE(server_connector_result);

        EXPECT_CALL(m_service_state_change_mock, Call(_, Service_state::available, _)).Times(1);

        client_connector = std::move(client_connector_result.value());
        server_connector =
            Disabled_server_connector::enable(std::move(server_connector_result.value()));
    }

    void TearDown() override {
        EXPECT_CALL(m_service_state_change_mock, Call(_, Service_state::not_available, _)).Times(1);
        Runtime_test::TearDown();
    }

    std::unique_ptr<Client_connector> client_connector;
    std::unique_ptr<Enabled_server_connector> server_connector;

    // Method mocks
    Method_call_credentials_callback_mock m_method_call_credentials_mock;
    Method_reply_callback_mock m_method_reply_mock;
};

TEST_F(Runtime_test, client_connector_construction_works) {
    auto const client_connector_result =
        runtime->make_client_connector(config, instance, make_client_callbacks());
    EXPECT_TRUE(client_connector_result);
}

TEST_F(Runtime_test, server_connector_construction_works) {
    auto const server_connector_result =
        runtime->make_server_connector(config, instance, make_server_callbacks());
    EXPECT_TRUE(server_connector_result);
}

TEST_F(Runtime_test, connection_setup_works) {
    auto const client_connector_result =
        runtime->make_client_connector(config, instance, make_client_callbacks());
    auto server_connector_result =
        runtime->make_server_connector(config, instance, make_server_callbacks());

    ASSERT_TRUE(client_connector_result);
    ASSERT_TRUE(server_connector_result);

    EXPECT_CALL(m_service_state_change_mock, Call(_, Service_state::available, _)).Times(1);

    auto const enabled_server_connector =
        Disabled_server_connector::enable(std::move(server_connector_result.value()));

    EXPECT_CALL(m_service_state_change_mock, Call(_, Service_state::not_available, _)).Times(1);
}

TEST_F(Connection_test, server_sends_event_which_is_received_by_the_client) {
    EXPECT_CALL(m_event_update_mock, Call(_, _, _)).Times(1);
    EXPECT_CALL(m_event_subscription_change_mock, Call(_, 0, Event_state::subscribed)).Times(1);
    ASSERT_TRUE(client_connector->subscribe_event(0, Event_mode::update));
    server_connector->update_event(0, empty_payload());
}

TEST_F(Connection_test, client_calls_method_and_gets_response) {
    Method_call_reply_data_opt pointer;
    EXPECT_CALL(m_method_call_mock, Call(_, 0, _, _, _))
        .WillOnce([&pointer](auto&, auto, auto, auto cb_data, auto) {
            pointer = std::move(cb_data);
            return nullptr;
        });
    auto const invocation = client_connector->call_method(
        0, empty_payload(),
        Method_call_reply_data{m_method_reply_mock.as_function(), std::nullopt});
    // ASSERT_TRUE(invocation);

    EXPECT_CALL(m_method_reply_mock, Call).Times(1);
    pointer->reply(Method_result{Application_return{empty_payload()}});
}

}  // namespace score::socom
