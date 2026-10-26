/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#include <gtest/gtest.h>

#include <packetrouter/ApplicationRouter.h>
#include <runtime/ApplicationManager.h>
#include <runtime/EventManager.h>

using namespace lgsomeip;

class EventFakeRouter final : public ApplicationRouter {
public:
    void init() override {}
    void start() override {}
    void stop() override {}
    void join() override {}

    void send_message(std::shared_ptr<MessageSD>) override {}

    void send_message(std::shared_ptr<MessageSOMEIP> message) override {
        ++message_count;
        last_message = std::move(message);
    }

    void send_message(MessageSOMEIP& message) override {
        ++message_count;
        last_message = std::make_shared<MessageSOMEIP>(message);
    }

#if defined(ENABLE_SOMEIP_IPC)
    void add_ipc_route(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, bool) override {}
    // Services missing from the configuration are routed over IPC, so record them like regular messages.
    void send_ipc_message(std::shared_ptr<MessageSOMEIP> message) override {
        ++message_count;
        last_message = std::move(message);
    }
    void send_ipc_message(MessageSOMEIP& message, bool) override {
        ++message_count;
        last_message = std::make_shared<MessageSOMEIP>(message);
    }
    void send_ipc_response_message(MessageSOMEIP& message, std::uint16_t, std::uint16_t) override {
        ++message_count;
        last_message = std::make_shared<MessageSOMEIP>(message);
    }
    void on_internal_request(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t) override {}
#endif // ENABLE_SOMEIP_IPC

    int message_count = 0;
    std::shared_ptr<MessageSOMEIP> last_message;
};

TEST(EventManager, notifies_enabled_update_on_change_event) {
    auto router = std::make_shared<EventFakeRouter>();
    ApplicationManager application("event-test", "", router);
    EventManager events(&application);

    events.add_event(0x1001, 0x0001, 0x0002, 0x01, false, 0);
    events.set_service_enabled(0x1001, 0x0001, 0x01, true);

    auto payload = std::make_shared<Payload>();
    const std::uint8_t value[] = {0x01, 0x02};
    payload->set_payload(value, sizeof(value));
    events.notify(0x1001, 0x0001, 0x0002, payload);

    ASSERT_EQ(router->message_count, 1);
    ASSERT_NE(router->last_message, nullptr);
    EXPECT_EQ(router->last_message->get_message_id(), 0x10010002U);
    EXPECT_EQ(router->last_message->get_instance_id(), 0x0001);
    EXPECT_EQ(router->last_message->get_payload_type()->get_payload_vector(), (std::vector<std::uint8_t>{0x01, 0x02}));
}

// SOME/IP spec 6.4.1 "update on change": an unchanged value is not sent again, for an event and a field alike.
static void expect_update_on_change(bool is_field) {
    auto router = std::make_shared<EventFakeRouter>();
    ApplicationManager application("event-test", "", router);
    EventManager events(&application);

    events.add_event(0x1001, 0x0001, 0x0002, 0x01, is_field, 0);
    events.set_service_enabled(0x1001, 0x0001, 0x01, true);

    auto payload = std::make_shared<Payload>();
    payload->append(static_cast<std::uint8_t>(0x01));
    events.notify(0x1001, 0x0001, 0x0002, payload);
    events.notify(0x1001, 0x0001, 0x0002, payload);
    EXPECT_EQ(router->message_count, 1);

    events.notify(0x1001, 0x0001, 0x0002, payload, 0, true);
    EXPECT_EQ(router->message_count, 2);

    auto changed = std::make_shared<Payload>();
    changed->append(static_cast<std::uint8_t>(0x02));
    events.notify(0x1001, 0x0001, 0x0002, changed);
    EXPECT_EQ(router->message_count, 3);
}

TEST(EventManager, update_on_change_event_is_sent_only_when_payload_changes_or_forced) {
    expect_update_on_change(false);
}

TEST(EventManager, update_on_change_field_is_sent_only_when_payload_changes_or_forced) {
    expect_update_on_change(true);
}

TEST(EventManager, cyclic_event_is_sent_on_flush_even_with_unchanged_payload) {
    auto router = std::make_shared<EventFakeRouter>();
    ApplicationManager application("event-test", "", router);
    EventManager events(&application);

    events.add_event(0x1001, 0x0001, 0x0002, 0x01, false, 100);
    events.set_service_enabled(0x1001, 0x0001, 0x01, true);

    auto payload = std::make_shared<Payload>();
    payload->append(static_cast<std::uint8_t>(0x01));
    events.notify(0x1001, 0x0001, 0x0002, payload);
    EXPECT_EQ(router->message_count, 0);

    events.notify(0x1001, 0x0001, 0x0002, payload, 0, false, true);
    EXPECT_EQ(router->message_count, 1);

    // Same payload again: must neither deadlock nor be skipped when flushed.
    events.notify(0x1001, 0x0001, 0x0002, payload, 0, false, true);
    EXPECT_EQ(router->message_count, 2);
}

TEST(EventManager, notify_one_targets_the_client_and_session_id_keeps_counting) {
    auto router = std::make_shared<EventFakeRouter>();
    ApplicationManager application("event-test", "", router);
    EventManager events(&application);

    events.add_event(0x1001, 0x0001, 0x0002, 0x01, false, 0);
    events.set_service_enabled(0x1001, 0x0001, 0x01, true);

    auto payload = std::make_shared<Payload>();
    payload->append(static_cast<std::uint8_t>(0x01));
    events.notify(0x1001, 0x0001, 0x0002, payload);
    EXPECT_EQ(router->last_message->get_request_id(), 0x00000001U);

    // The target client goes into the upper 16 bits and the session ID still counts up by one.
    auto changed = std::make_shared<Payload>();
    changed->append(static_cast<std::uint8_t>(0x02));
    events.notify(0x1001, 0x0001, 0x0002, changed, 0x1234);
    EXPECT_EQ(router->last_message->get_request_id(), 0x12340002U);

    // The target is not sticky: the next notification addresses every subscriber again.
    events.notify(0x1001, 0x0001, 0x0002, payload);
    EXPECT_EQ(router->last_message->get_request_id(), 0x00000003U);
}

TEST(EventManager, ignores_duplicate_event_registration) {
    auto router = std::make_shared<EventFakeRouter>();
    ApplicationManager application("event-test", "", router);
    EventManager events(&application);

    events.add_event(0x1001, 0x0001, 0x0002);
    events.add_event(0x1001, 0x0001, 0x0002);
    events.set_service_enabled(0x1001, 0x0001, 0xff, true);

    auto payload = std::make_shared<Payload>();
    payload->append(static_cast<std::uint8_t>(0x01));
    events.notify(0x1001, 0x0001, 0x0002, payload);

    EXPECT_EQ(router->message_count, 1);
}

TEST(EventManager, sends_initial_event_for_field_and_removes_event) {
    auto router = std::make_shared<EventFakeRouter>();
    ApplicationManager application("event-test", "", router);
    EventManager events(&application);

    events.add_event(0x1001, 0x0001, 0x0002, 0x01, true, 100);
    events.set_service_enabled(0x1001, 0x0001, 0x01, true);
    auto payload = std::make_shared<Payload>();
    payload->append(static_cast<std::uint8_t>(0x01));
    events.notify(0x1001, 0x0001, 0x0002, payload);
    events.notify_initial_event(0x1001, 0x0001, 0x0002);

    ASSERT_EQ(router->message_count, 1);
    ASSERT_NE(router->last_message, nullptr);
    EXPECT_EQ(router->last_message->get_message_id(), 0x10010002U);

    events.remove_event(0x1001, 0x0001, 0x0002);
    events.notify(0x1001, 0x0001, 0x0002, std::make_shared<Payload>());
    EXPECT_EQ(router->message_count, 1);
}

TEST(EventManager, start_stop_join_is_idempotent) {
    auto router = std::make_shared<EventFakeRouter>();
    ApplicationManager application("event-test", "", router);
    EventManager events(&application);

    events.start();
    events.start();
    events.stop();
    events.stop();
    events.join();
}
