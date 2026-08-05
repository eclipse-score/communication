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
#include "score/socom/event.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/server_t.hpp"
#include "score/socom/single_connection_test_fixture.hpp"
#include "score/socom/temporary_event_subscription.hpp"
#include "score/socom/utilities.hpp"
#include "score/socom/vector_payload.hpp"

using ::testing::_;

namespace score::socom {

using EventTest = SingleConnectionTest;

TEST(Event_test, data_types) {
    EXPECT_TRUE((std::is_same_v<Event_id, std::uint16_t>));

    EXPECT_EQ(std::is_enum_v<Event_mode>, true);
    EXPECT_EQ(static_cast<uint8_t>(Event_mode::update), 0x00U);
    EXPECT_EQ(static_cast<uint8_t>(Event_mode::update_and_initial_value), 0x01U);

    EXPECT_EQ(std::is_enum_v<Event_state>, true);
    EXPECT_EQ(static_cast<uint8_t>(Event_state::unsubscribed), 0x00U);
    EXPECT_EQ(static_cast<uint8_t>(Event_state::subscribed), 0x01U);
}

TEST_F(EventTest, FirstEventSubscriptionCallsOnEventSubscriptionChange) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    auto const& unsubscribed =
        server.expect_on_event_subscription_change(event_id, Event_state::unsubscribed);

    {
        auto const& subscribed =
            server.expect_on_event_subscription_change(event_id, Event_state::subscribed);

        auto const sub = client0.create_event_subscription(event_id);

        wait_for_atomics(subscribed);

        EXPECT_FALSE(unsubscribed);
    }
    wait_for_atomics(unsubscribed);
}

TEST_F(EventTest, ServerSendsEventWhichIsReceivedBySubscribedClient) {
    std::vector<Payload> payloads{};
    payloads.emplace_back(empty_payload());
    payloads.emplace_back(clone_payload(real_payload));

    for (auto const& payload : payloads) {
        Server_data server{connector_factory};
        Client_data client0{connector_factory};

        server.expect_event_subscription(event_id);
        auto const sub = client0.create_event_subscription(event_id);

        client0.expect_event_update(event_id, payload);

        server.update_event(event_id, payload);
    }
}

TEST_F(EventTest, ClientRequestsEventUpdateAndReceivesEventUpdate) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    server.expect_event_subscription(event_id);
    auto const sub = client0.create_event_subscription(event_id);

    server.expect_and_respond_update_event_request(event_id, empty_payload());

    wait_for_atomics(client0.expect_and_request_event_update(event_id, empty_payload()));
}

TEST_F(EventTest, ClientRequestsEventUpdateAndServerRespondsWithUpdateRequestedEvent) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    server.expect_event_subscription(event_id);
    auto const sub = client0.create_event_subscription(event_id);

    auto const& update_requested = server.expect_update_event_request(event_id);

    client0.request_event_update(event_id);
    auto const& update_received = client0.expect_requested_event_update(event_id, real_payload);
    wait_for_atomics(update_requested);
    server.update_requested_event(event_id, real_payload);
    wait_for_atomics(update_received);

    auto const& update_requested1 = server.expect_update_event_request(event_id);
    client0.request_event_update(event_id);
    wait_for_atomics(update_requested1);
}

TEST_F(EventTest, ClientRequestsEventUpdateAndServerConnectorRespondsWithUpdateEvent) {
    Server_connector_callbacks_mock callbacks;
    Enabled_server_connector::Uptr server = connector_factory.create_and_enable(callbacks);
    Client_data client{connector_factory};

    EXPECT_CALL(callbacks, on_event_subscription_change(_, _, _)).Times(2U);

    auto subscription = client.create_event_subscription(event_id);
    client.subscribe_event(event_id);

    EXPECT_CALL(callbacks, on_event_update_request(_, event_id))
        .WillOnce([this](Enabled_server_connector& connector, Event_id const& eid) {
            auto const event_mode = connector.get_event_mode(eid);
            auto const expected_mode = score::Result<Event_mode>{Event_mode::update};
            EXPECT_EQ(expected_mode, event_mode);

            connector.update_event(eid, clone_payload(real_payload));
        });

    auto const& expect_event_update = client.expect_event_update(event_id, real_payload);
    client.request_event_update(event_id);
    wait_for_atomics(expect_event_update);
}

TEST_F(EventTest, ClientRequestsEventUpdateWithDataAndReceivesEventUpdate) {
    Server_data server{connector_factory};
    server.expect_event_subscription(event_id);
    Client_data client0{connector_factory};

    auto const sub = client0.create_event_subscription(event_id);

    server.expect_and_respond_update_event_request(event_id, real_payload);
    wait_for_atomics(client0.expect_and_request_event_update(event_id, real_payload));
}

TEST_F(EventTest, ServerUpdatesRequestedEventWithoutRequestAndClientReceivesNoUpdate) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    auto const& subscribed =
        server.expect_on_event_subscription_change(event_id, Event_state::subscribed);
    auto const& unsubscribed =
        server.expect_on_event_subscription_change(event_id, Event_state::unsubscribed);
    {
        auto const sub0 = client0.create_event_subscription(event_id);
        wait_for_atomics(subscribed);

        server.update_requested_event(event_id, empty_payload());
    }
    wait_for_atomics(unsubscribed);
}

TEST_F(EventTest, ClientRequestsManyEventUpdatesAndReceivesEventUpdates) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    auto const& subscribed =
        server.expect_on_event_subscription_change(event_id, Event_state::subscribed);
    server.expect_on_event_subscription_change_nosync(event_id, Event_state::unsubscribed);
    auto const sub0 = client0.create_event_subscription(event_id);
    wait_for_atomics(subscribed);

    for (auto i = size_t{0}; i < 100; i++) {
        server.expect_and_respond_update_event_request(event_id, empty_payload());
        auto const& event_received =
            client0.expect_and_request_event_update(event_id, empty_payload());
        wait_for_atomics(event_received);
    }
}

TEST_F(EventTest,
       ClientRequestsManyEventUpdatesWhichAreCachedByTheMiddlewareAndClientReceivesOneEventUpdate) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    auto const& subscribed =
        server.expect_on_event_subscription_change(event_id, Event_state::subscribed);
    server.expect_on_event_subscription_change_nosync(event_id, Event_state::unsubscribed);
    auto const sub0 = client0.create_event_subscription(event_id);
    wait_for_atomics(subscribed);

    auto const& event_reveived = client0.expect_requested_event_update(event_id, empty_payload());

    auto const& server_received_request = server.expect_update_event_request(event_id);
    for (auto i = size_t{0}; i < size_t{100}; i++) {
        client0.request_event_update(event_id);
    }
    wait_for_atomics(server_received_request);

    server.update_requested_event(event_id, empty_payload());
    wait_for_atomics(event_reveived);
}

TEST_F(EventTest, ClientSubscribesEventWithUpdateAndInitialValue) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    server.expect_event_subscription(event_id);
    auto const sub = client0.create_event_subscription(
        server, event_id, Temporary_event_subscription::Brokenness::no_server_reponse);
}

TEST_F(EventTest, ClientSubscribesEventWithUpdateAndInitialValueFollowedByRequestEventUpdate) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    server.expect_event_subscription(event_id);
    auto const sub = client0.create_event_subscription(
        server, event_id, Temporary_event_subscription::Brokenness::no_server_reponse);

    client0.request_event_update(event_id);
}

TEST_F(EventTest, MiddlewareCachesEventUpdateRequestsUntilServerAnswers) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    server.expect_event_subscription(event_id);
    auto const sub = client0.create_event_subscription(
        server, event_id, Temporary_event_subscription::Brokenness::no_server_reponse);

    auto const& callback_called = client0.expect_requested_event_update(event_id, empty_payload());
    server.update_requested_event(event_id, empty_payload());
    wait_for_atomics(callback_called);

    auto const& event_requested = server.expect_update_event_request(event_id);
    client0.request_event_update(event_id);
    wait_for_atomics(event_requested);
}

TEST_F(EventTest, AllocateEventPayloadWithOutOfBoundsEventIdReturnsLogicErrorIdOutOfRange) {
    Server_data server{connector_factory};

    auto payload = server.get_connector().allocate_event_payload(event_id + 1);
    EXPECT_FALSE(payload);
    EXPECT_EQ(payload.error(), Server_connector_error::logic_error_id_out_of_range);
}

TEST_F(EventTest,
       AllocateEventPayloadWithoutSubscribedClientReturnsNoClientSubscribedForEventError) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    auto payload = server.get_connector().allocate_event_payload(event_id);
    EXPECT_FALSE(payload);
    EXPECT_EQ(payload.error(),
              Server_connector_error::runtime_error_no_client_subscribed_for_event);
}

TEST_F(EventTest, AllocateEventPayloadWithSubscribedClientReturnsPayload) {
    Server_data server{connector_factory};
    Client_data client0{connector_factory};

    server.expect_event_subscription(event_id);
    auto const sub = client0.create_event_subscription(event_id);

    score::Result<Writable_payload> wpayload{make_writable_vector_payload(64)};
    auto const* const ptr = wpayload->data().data();

    auto const& expect_event_payload_allocation =
        client0.expect_event_payload_allocation(event_id, std::move(wpayload));

    auto payload = server.get_connector().allocate_event_payload(event_id);
    EXPECT_TRUE(payload);
    EXPECT_EQ(payload->data().data(), ptr);
    wait_for_atomics(expect_event_payload_allocation);
}

}  // namespace score::socom
