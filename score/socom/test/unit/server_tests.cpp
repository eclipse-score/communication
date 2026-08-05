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

#include <array>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "score/socom/client_connector.hpp"
#include "score/socom/clients_t.hpp"
#include "score/socom/connector_factory.hpp"
#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/server_t.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/single_connection_test_fixture.hpp"
#include "score/socom/socom_mocks.hpp"
#include "score/socom/utilities.hpp"

using score::Blank;
using score::Result;
using ::testing::_;
using ::testing::StaticAssertTypeEq;

namespace score::socom {

class ServerConnectorTest : public SingleConnectionTest {
   protected:
    Server_connector_callbacks_mock callbacks;

    score::Result<Blank> ok;
    score::Result<Disabled_server_connector::Uptr> const callback_missing =
        score::MakeUnexpected(Construction_error::callback_missing);

    Event_subscription_change_callback_mock esccb;
    Event_request_update_callback_mock eruc;
    Method_call_credentials_callback_mock mccb;
    Method_call_payload_allocate_callback_mock mpacb;
};

TEST_F(ServerConnectorTest, ConstructDestruct) {
    auto scd = connector_factory.create_server_connector_with_result(callbacks);

    StaticAssertTypeEq<decltype(scd), Result<Disabled_server_connector::Uptr>>();
    EXPECT_TRUE(scd.has_value());
}

TEST_F(ServerConnectorTest, ConfigurationAndInstanceIsAsExpected) {
    Disabled_server_connector::Uptr scd{connector_factory.create_server_connector(callbacks)};

    auto const& service_cfg_disabled = scd->get_configuration();
    EXPECT_EQ(service_cfg_disabled.get_interface(), service_interface);
    EXPECT_EQ(service_cfg_disabled.get_num_methods(), num_methods);
    EXPECT_EQ(service_cfg_disabled.get_num_events(), num_events);

    EXPECT_EQ(scd->get_service_instance(), service_instance);

    Enabled_server_connector::Uptr sce{Disabled_server_connector::enable(std::move(scd))};

    auto const validate_configuration = [this](auto const& sce) {
        auto const& service_cfg_enabled = sce.get_configuration();
        EXPECT_EQ(service_cfg_enabled.get_interface(), service_interface);
        EXPECT_EQ(service_cfg_enabled.get_num_methods(), num_methods);
        EXPECT_EQ(service_cfg_enabled.get_num_events(), num_events);
    };

    auto const validate_service_instance = [this](auto const& sce) {
        EXPECT_EQ(sce.get_service_instance(), service_instance);
    };

    EXPECT_CALL(callbacks, on_event_subscription_change(_, _, _))
        .Times(2)
        .WillRepeatedly([&validate_configuration, &validate_service_instance](
                            auto& sce, auto /* id */, auto /* state */) {
            validate_configuration(sce);
            validate_service_instance(sce);
        });
    Client_data client0{connector_factory};
    auto const subscription = client0.create_event_subscription(event_id);
}

TEST_F(ServerConnectorTest, DisabledServerConnectorMoveConstructorWorks) {
    Disabled_server_connector::Uptr from{connector_factory.create_server_connector(callbacks)};
    Disabled_server_connector::Uptr to{std::move(from)};

    auto esc = Disabled_server_connector::enable(std::move(to));
    EXPECT_EQ(ok, esc->update_event(event_id, clone_payload(real_payload)));
}

TEST_F(ServerConnectorTest, DisabledServerConnectorMoveAssignmentWorks) {
    Disabled_server_connector::Uptr from{connector_factory.create_server_connector(callbacks)};
    Disabled_server_connector::Uptr to{nullptr};
    to = std::move(from);

    auto esc = Disabled_server_connector::enable(std::move(to));
    EXPECT_EQ(ok, esc->update_event(event_id, clone_payload(real_payload)));
}

TEST_F(ServerConnectorTest, Enable) {
    auto scd = connector_factory.create_server_connector(callbacks);

    auto sce = Disabled_server_connector::enable(std::move(scd));

    StaticAssertTypeEq<decltype(sce), Enabled_server_connector::Uptr>();
}

TEST_F(ServerConnectorTest, ConstructDestructNoCallbacksReturnsCallbackMissing) {
    auto scd = connector_factory.create_server_connector_with_result(
        Optional_reference<Server_connector_callbacks_mock>{});

    StaticAssertTypeEq<decltype(scd), score::Result<Disabled_server_connector::Uptr>>();

    EXPECT_EQ(callback_missing, scd);
}

TEST_F(ServerConnectorTest, WhenCallbackMissingCreationReturnsCallbackMissing) {
    std::array<Disabled_server_connector::Callbacks, 4> server_callbacks_array = {
        Disabled_server_connector::Callbacks{nullptr, esccb.as_function(), eruc.as_function(),
                                             mpacb.as_function()},
        Disabled_server_connector::Callbacks{mccb.as_function(), nullptr, eruc.as_function(),
                                             mpacb.as_function()},
        Disabled_server_connector::Callbacks{mccb.as_function(), esccb.as_function(), nullptr,
                                             mpacb.as_function()},
        Disabled_server_connector::Callbacks{mccb.as_function(), esccb.as_function(),
                                             eruc.as_function(), nullptr},
    };

    for (auto& server_callbacks : server_callbacks_array) {
        auto scd =
            connector_factory.create_server_connector_with_result(std::move(server_callbacks));

        EXPECT_EQ(callback_missing, scd);
    }
}

TEST_F(ServerConnectorTest, NoSubscribedClientReturnsUpdate) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    EXPECT_EQ(server.get_event_mode(event_id), Event_mode::update);
}

class ServerConnectorDeathTest : public ServerConnectorTest {
   protected:
    std::unique_ptr<Enabled_server_connector> server;
    Client_data client0;
    const ::std::uint32_t deadlock_detected_by_destroying_server_connector_inside_callback_id =
        0x50000004U;
    std::string const expected_message{
        "SOCom error: A callback causes the Enabled_server_connector"};

    ServerConnectorDeathTest()
        : server{connector_factory.create_and_enable(callbacks)}, client0{connector_factory} {
        // triggered by server self destruction.
        client0.expect_service_state_change(Service_state::not_available);
    }

    ~ServerConnectorDeathTest() override { server.reset(); }
};

#ifdef WITH_SOCOM_DEADLOCK_DETECTION

TEST_F(ServerConnectorDeathTest,
       ServerDeletionByOnEventSubscriptionChangeResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        auto const self_destruct = [this](Enabled_server_connector& /* server */, Event_id /* id */,
                                          Event_state /* state */) { server.reset(); };
        EXPECT_CALL(callbacks, on_event_subscription_change(_, _, _)).WillOnce(self_destruct);

        auto const subscription = client0.create_event_subscription(event_id);
    };

    EXPECT_DEATH(el_failure(), expected_message);
}

TEST_F(ServerConnectorDeathTest,
       ServerDeletionByOnEventSubscriptionChangeAtUnsubscriptionResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        auto const self_destruct = [this](Enabled_server_connector& /* server */, Event_id /* id */,
                                          Event_state /* state */) { server.reset(); };
        EXPECT_CALL(callbacks, on_event_subscription_change(_, _, Event_state::subscribed));

        auto const subscription = client0.create_event_subscription(event_id);
        EXPECT_CALL(callbacks, on_event_subscription_change(_, _, Event_state::unsubscribed))
            .WillOnce(self_destruct);
    };
    EXPECT_DEATH(el_failure(), expected_message);
}

TEST_F(ServerConnectorDeathTest,
       ServerDeletionByOnEventSubscriptionChangeWithInitialDataResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        auto const self_destruct = [this](Enabled_server_connector& /* server */,
                                          Event_id /* id */) { server.reset(); };
        EXPECT_CALL(callbacks, on_event_subscription_change(_, _, _)).Times(2);
        EXPECT_CALL(callbacks, on_event_update_request(_, _)).WillOnce(self_destruct);
        client0.subscribe_event(event_id, Event_mode::update_and_initial_value);
    };
    EXPECT_DEATH(el_failure(), expected_message);
}

TEST_F(ServerConnectorDeathTest,
       ServerDeletionByOnEventUpdateRequestResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        EXPECT_CALL(callbacks, on_event_subscription_change(_, _, _));
        auto const subscription = client0.create_event_subscription(event_id);

        auto const self_destruct = [this](Enabled_server_connector& /* server */,
                                          Event_id /* id */) { server.reset(); };
        EXPECT_CALL(callbacks, on_event_update_request(_, _)).WillOnce(self_destruct);

        client0.request_event_update(event_id);
    };
    EXPECT_DEATH(el_failure(), expected_message);
}

TEST_F(ServerConnectorDeathTest, ServerDeletionByOnMethodCallResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        auto const self_destruct = [this](Enabled_server_connector& /*server*/,
                                          Method_id /*method_id*/, Payload const& /*payload*/,
                                          Method_call_reply_data_opt /*reply*/) {
            server.reset();
            return nullptr;
        };

        EXPECT_CALL(callbacks, on_method_call(_, _, _, _)).WillOnce(self_destruct);
        client0.call_method_fire_and_forget(method_id, empty_payload());
    };
    EXPECT_DEATH(el_failure(), expected_message);
}

TEST_F(ServerConnectorDeathTest, ServerDeletionByOnMethodCallReplyResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        auto const empty_reply = [](Enabled_server_connector& /*server*/, Method_id /*method_id*/,
                                    Payload const& /*payload*/, Method_call_reply_data_opt reply) {
            reply->reply(Method_result{Application_return{}});
            return nullptr;
        };

        EXPECT_CALL(callbacks, on_method_call(_, _, _, _)).WillOnce(empty_reply);

        Method_reply_callback reset_server = [this](Method_result const& /*mr*/) {
            server.reset();
        };

        client0.call_method(method_id, empty_payload(), std::move(reset_server));
    };
    EXPECT_DEATH(el_failure(), expected_message);
}

#endif

class ServerConnectorGetEventModeTest : public SingleConnectionTest {
   protected:
    Server_data server{connector_factory};
    Client_data client0{connector_factory};
    std::atomic_bool const& wait_subscribed{
        server.expect_on_event_subscription_change(event_id, Event_state::subscribed)};

    void SetUp() override {
        SingleConnectionTest::SetUp();
        server.expect_on_event_subscription_change_nosync(event_id, Event_state::unsubscribed);
    }
};

TEST_F(ServerConnectorGetEventModeTest, SubscribedClientReturnsUpdate) {
    auto const sub = client0.create_event_subscription(event_id);

    wait_for_atomics(wait_subscribed);

    EXPECT_EQ(server.get_event_mode(event_id), Event_mode::update);
}

TEST_F(ServerConnectorGetEventModeTest, SubscribedClientReturnsUpdateAndInitialValue) {
    auto const sub = client0.create_event_subscription(
        server, event_id, Temporary_event_subscription::Brokenness::no_server_reponse);

    wait_for_atomics(wait_subscribed);

    EXPECT_EQ(server.get_event_mode(event_id), Event_mode::update_and_initial_value);
}

class ServerConnectorOutOfBoundsTest : public SingleConnectionTest {
   protected:
    Server_connector_callbacks_mock callbacks;
    Enabled_server_connector::Uptr server = connector_factory.create_and_enable(callbacks);

    score::Result<Blank> out_of_range =
        score::MakeUnexpected(Server_connector_error::logic_error_id_out_of_range);
};

TEST_F(ServerConnectorOutOfBoundsTest, UpdateEventWithOutOfBoundsIndexReturnsOutOfRange) {
    EXPECT_EQ(out_of_range, (server->update_event(max_event_id + 1, empty_payload())));
}

TEST_F(ServerConnectorOutOfBoundsTest, UpdateRequestedEventWithOutOfBoundsIndexReturnsOutOfRange) {
    EXPECT_EQ(out_of_range, (server->update_requested_event(max_event_id + 1, empty_payload())));
}

TEST_F(ServerConnectorOutOfBoundsTest, OutOfBoundsIndexReturnsOutOfRange) {
    auto const out_of_range =
        score::MakeUnexpected(Server_connector_error::logic_error_id_out_of_range);
    EXPECT_EQ(out_of_range, (server->get_event_mode(max_event_id + 1)));
}

}  // namespace score::socom
