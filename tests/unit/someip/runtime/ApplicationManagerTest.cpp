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

#include <condition_variable>
#include <mutex>
#include <message/MessageComposer.h>
#include <packetrouter/ApplicationRouter.h>
#include <runtime/ApplicationManager.h>

#include <set>

using namespace lgsomeip;

class FakeApplicationRouter final : public ApplicationRouter {
public:
    void init() override {
        initialized = true;
    }

    void start() override {
        started = true;
    }

    void stop() override {
        stopped = true;
    }

    void join() override {
        joined = true;
    }

    void send_message(std::shared_ptr<MessageSD> message) override {
        ++sd_message_count;
        last_sd_message = std::move(message);
    }

    void send_message(std::shared_ptr<MessageSOMEIP> message) override {
        ++someip_message_count;
        last_someip_message = std::move(message);
    }

    void send_message(MessageSOMEIP& message) override {
        ++someip_message_count;
        last_someip_message = std::make_shared<MessageSOMEIP>(message);
    }

#if defined(ENABLE_SOMEIP_IPC)
    void add_ipc_route(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, bool) override {}
    void send_ipc_message(std::shared_ptr<MessageSOMEIP>) override {}
    void send_ipc_message(MessageSOMEIP&, bool) override {}
    void send_ipc_response_message(MessageSOMEIP&, std::uint16_t, std::uint16_t) override {}
    void on_internal_request(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t) override {}
#endif // ENABLE_SOMEIP_IPC

    bool initialized = false;
    bool started = false;
    bool stopped = false;
    bool joined = false;
    int sd_message_count = 0;
    int someip_message_count = 0;
    std::shared_ptr<MessageSD> last_sd_message;
    std::shared_ptr<MessageSOMEIP> last_someip_message;
};

std::shared_ptr<MessageSD> make_offer_service_message(std::uint16_t service, std::uint16_t instance, std::uint8_t major,
                                                      std::uint32_t minor, std::uint32_t ttl) {
    auto message = MessageBuilder::create<SOMEIPSD>();
    SDEntry entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    entry.set_service_id(service);
    entry.set_instance_id(instance);
    entry.set_major_version(major);
    entry.set_minor_version(minor);
    entry.set_ttl(ttl);
    MessageComposer::add_entry(message, &entry);
    return message;
}

TEST(ApplicationManager, uses_injected_router_for_lifecycle) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);

    application_manager.init();
    application_manager.start();
    application_manager.stop();
    application_manager.join();

    EXPECT_TRUE(router->initialized);
    EXPECT_TRUE(router->started);
    EXPECT_TRUE(router->stopped);
    EXPECT_TRUE(router->joined);
}

TEST(ApplicationManager, forwards_messages_to_injected_router) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    auto request = MessageBuilder::create_request_message(0x1001, 0x0001);

    application_manager.send(request);

    EXPECT_EQ(router->someip_message_count, 1);
}

TEST(ApplicationManager, offer_service_sends_offer_entry_with_max_ttl) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    application_manager.offer_service(0x1001, 0x0002, 0x01, 0x00000003);

    ASSERT_NE(router->last_sd_message, nullptr);
    ASSERT_EQ(router->last_sd_message->entries().size(), 1U);
    auto& entry = router->last_sd_message->entries().front();
    EXPECT_EQ(entry.get_type(), SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    EXPECT_EQ(entry.get_service_id(), 0x1001);
    EXPECT_EQ(entry.get_instance_id(), 0x0002);
    EXPECT_EQ(entry.get_major_version(), 0x01);
    EXPECT_EQ(entry.get_minor_version(), 0x00000003U);
    EXPECT_EQ(entry.get_ttl(), 0x00FFFFFFU);
}

TEST(ApplicationManager, stop_offer_service_after_offer_sends_offer_entry_with_zero_ttl) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    application_manager.offer_service(0x1001, 0x0002, 0x01, 0x00000003);
    application_manager.stop_offer_service(0x1001, 0x0002, 0x01, 0x00000003);

    ASSERT_NE(router->last_sd_message, nullptr);
    ASSERT_EQ(router->last_sd_message->entries().size(), 1U);
    auto& entry = router->last_sd_message->entries().front();
    EXPECT_EQ(entry.get_type(), SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    EXPECT_EQ(entry.get_ttl(), 0U);
    EXPECT_EQ(router->sd_message_count, 2);
}

TEST(ApplicationManager, find_service_sends_find_entry_with_ttl_on) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    application_manager.find_service(0x1001, 0x0002, 0x01, 0x00000003);

    ASSERT_NE(router->last_sd_message, nullptr);
    ASSERT_EQ(router->last_sd_message->entries().size(), 1U);
    auto& entry = router->last_sd_message->entries().front();
    EXPECT_EQ(entry.get_type(), SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
    EXPECT_EQ(entry.get_service_id(), 0x1001);
    EXPECT_EQ(entry.get_instance_id(), 0x0002);
    EXPECT_EQ(entry.get_ttl(), SOMEIP_DEFAULT_TTL_ON);
}

TEST(ApplicationManager, request_service_is_idempotent_when_already_requested) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    application_manager.request_service(0x1001, 0x0002, 0x01, 0x00000003);
    application_manager.request_service(0x1001, 0x0002, 0x01, 0x00000003);

    EXPECT_EQ(router->sd_message_count, 1);
}

TEST(ApplicationManager, is_service_available_returns_false_when_not_requested) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    EXPECT_FALSE(application_manager.is_service_available(0x1001, 0x0002, 0x01, 0x00000003));
}

TEST(ApplicationManager, stop_offer_service_with_any_major_and_minor_stops_all_offers) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    application_manager.offer_service(0x1001, 0x0002, 0x01, 0x00000003);
    application_manager.offer_service(0x1001, 0x0002, 0x02, 0x00000004);
    application_manager.stop_offer_service(0x1001, 0x0002, SOMEIP_DEFAULT_ANY_MAJOR, SOMEIP_DEFAULT_ANY_MINOR);

    ASSERT_NE(router->last_sd_message, nullptr);
    ASSERT_EQ(router->last_sd_message->entries().size(), 1U);
    EXPECT_EQ(router->last_sd_message->entries().front().get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
    EXPECT_EQ(router->sd_message_count, 4);
}

TEST(ApplicationManager, release_service_sends_find_service_with_ttl_off) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    application_manager.request_service(0x1001, 0x0002, 0x01, 0x00000003);
    application_manager.release_service(0x1001, 0x0002);

    ASSERT_NE(router->last_sd_message, nullptr);
    ASSERT_EQ(router->last_sd_message->entries().size(), 1U);
    EXPECT_EQ(router->last_sd_message->entries().front().get_type(), SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
    EXPECT_EQ(router->last_sd_message->entries().front().get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
    EXPECT_EQ(router->sd_message_count, 2);
}

TEST(ApplicationManager, are_service_available_reports_requested_service_when_unavailable) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    available_t available;
    EXPECT_FALSE(application_manager.are_service_available(available, 0x1001, 0x0002, 0x01, 0x00000003));
    ASSERT_EQ(available[0x1001][0x0002][0x01], 0x00000003U);
}

TEST(ApplicationManager, offer_service_updates_availability_state) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();
    application_manager.find_service(0x1001, 0x0002, 0x01, 0x00000003);

    auto offer = make_offer_service_message(0x1001, 0x0002, 0x01, 0x00000003, SOMEIP_DEFAULT_TTL_ON);
    application_manager.on_offer_service(offer);

    EXPECT_TRUE(application_manager.is_service_available(0x1001, 0x0002, 0x01, 0x00000003));
    available_t available;
    EXPECT_TRUE(application_manager.are_service_available(available, 0x1001, 0x0002, 0x01, 0x00000003));
    ASSERT_EQ(available[0x1001][0x0002][0x01], 0x00000003U);
}

TEST(ApplicationManager, offer_event_notifies_and_stops_after_event_removal) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();
    application_manager.offer_service(0x1001, 0x0002, 0x01, 0x00000003);
    application_manager.offer_event(0x1001, 0x0002, 0x0003, {0x0004});

    auto payload = std::make_shared<Payload>();
    payload->append(static_cast<std::uint8_t>(0x01));
    application_manager.notify(0x1001, 0x0002, 0x0003, payload);

    ASSERT_EQ(router->someip_message_count, 1);
    ASSERT_NE(router->last_someip_message, nullptr);
    EXPECT_EQ(router->last_someip_message->get_message_id(), 0x10010003U);

    application_manager.stop_offer_event(0x1001, 0x0002, 0x0003);
    application_manager.notify(0x1001, 0x0002, 0x0003, payload);

    EXPECT_EQ(router->someip_message_count, 1);
}

TEST(ApplicationManager, subscribe_and_unsubscribe_send_sd_messages_for_available_service) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    application_manager.request_event(0x1001, 0x0002, 0x0003, {0x0004});
    application_manager.find_service(0x1001, 0x0002, 0x01, 0x00000003);
    application_manager.on_offer_service(
        make_offer_service_message(0x1001, 0x0002, 0x01, 0x00000003, SOMEIP_DEFAULT_TTL_ON));
    application_manager.subscribe(0x1001, 0x0002, 0x0004, 0x01, 0x0003);

    ASSERT_EQ(router->sd_message_count, 2);
    ASSERT_NE(router->last_sd_message, nullptr);
    ASSERT_EQ(router->last_sd_message->entries().size(), 1U);
    EXPECT_EQ(router->last_sd_message->entries().front().get_type(), SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID);
    EXPECT_EQ(router->last_sd_message->entries().front().get_ttl(), SOMEIP_DEFAULT_TTL_ON);

    application_manager.unsubscribe(0x1001, 0x0002, 0x0004);

    ASSERT_EQ(router->sd_message_count, 3);
    EXPECT_EQ(router->last_sd_message->entries().front().get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
}

TEST(ApplicationManager, application_state_notifies_handler_and_reannounces_services) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();
    application_manager.offer_service(0x1001, 0x0002, 0x01, 0x00000003);
    application_manager.request_service(0x1002, 0x0003, 0x02, 0x00000004);

    std::vector<std::uint16_t> application_states;
    std::vector<bool> availability_states;
    application_manager.register_application_state_handler(
        [&](std::uint16_t state) { application_states.push_back(state); });
    application_manager.register_availability_handler(
        0x1002, 0x0003, [&](std::uint16_t, std::uint16_t, bool available) { availability_states.push_back(available); },
        0x02, 0x00000004);

    application_manager.on_application_state(true);
    EXPECT_EQ(application_states, (std::vector<std::uint16_t>{SOMEIP_APPLICATION_REGISTERED}));
    EXPECT_EQ(router->sd_message_count, 4);

    application_manager.on_application_state(false);
    EXPECT_EQ(application_states,
              (std::vector<std::uint16_t>{SOMEIP_APPLICATION_REGISTERED, SOMEIP_APPLICATION_DEREGISTERED}));
    EXPECT_EQ(availability_states, (std::vector<bool>{false}));

    application_manager.unregister_application_state_handler();
    application_manager.on_application_state(true);
    EXPECT_EQ(router->sd_message_count, 6);
}

TEST(ApplicationManager, handler_registration_and_removal_paths_are_safe) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);

    application_manager.register_message_handler(0x1001, 0x0002, 0x0003, [](std::shared_ptr<Message>) {});
    application_manager.unregister_message_handler(0x1001, 0x0002, 0x0003);

    application_manager.register_subscription_handler(0x1001, 0x0002, 0x0004, [](std::uint16_t, bool) { return true; });
    application_manager.register_async_subscription_handler(
        0x1001, 0x0002, 0x0004, [](std::uint16_t, bool, std::function<void(const bool)> callback) { callback(true); });
    application_manager.unregister_subscription_handler(0x1001, 0x0002, 0x0004);
    application_manager.unregister_async_subscription_handler(0x1001, 0x0002, 0x0004);

    application_manager.register_subscription_status_handler(
        0x1001, 0x0002, 0x0004, 0x0005,
        [](std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t) {}, false);
    application_manager.unregister_subscription_status_handler(0x1001, 0x0002, 0x0004, 0x0005);

    application_manager.register_subscription_error_handler(0x1001, 0x0002, 0x0004, [](std::uint16_t) {});
    application_manager.unregister_subscription_error_handler(0x1001, 0x0002, 0x0004);

    application_manager.register_availability_handler(0x1001, 0x0002, [](std::uint16_t, std::uint16_t, bool) {});
    application_manager.unregister_availability_handler(0x1001, 0x0002);
    application_manager.register_availability_handler(0x1001, 0x0002, nullptr);
}

TEST(ApplicationManager, dispatches_request_to_registered_provider_handler) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();
    application_manager.offer_service(0x1001, 0x0002, 0x01, 0x00000003);

    std::mutex callback_mutex;
    std::condition_variable callback_condition;
    int callback_count = 0;
    application_manager.register_message_handler(
        0x1001, 0x0002, 0x0003,
        [&](std::shared_ptr<Message>) {
            {
                std::lock_guard<std::mutex> lock(callback_mutex);
                ++callback_count;
            }
            callback_condition.notify_all();
        },
        true);

    auto request = MessageBuilder::create_request_message(0x1001, 0x0003, 0x01);
    request->set_instance_id(0x0002);
    application_manager.on_message(request);

    std::unique_lock<std::mutex> lock(callback_mutex);
    ASSERT_TRUE(callback_condition.wait_for(lock, std::chrono::seconds(1), [&] { return callback_count == 1; }));
}

TEST(ApplicationManager, sends_error_when_request_has_no_registered_handler) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();

    auto request = MessageBuilder::create_request_message(0x1001, 0x0003, 0x01);
    request->set_instance_id(0x0002);
    request->set_request_id(0x00090001);
    application_manager.on_message(request);

    ASSERT_NE(router->last_someip_message, nullptr);
    EXPECT_EQ(router->last_someip_message->get_message_type(), SOMEIP_MESSAGE_TYPE::ERROR);
    EXPECT_EQ(router->last_someip_message->get_return_code(), SOMEIP_RETURN_CODE::E_WRONG_INTERFACE_VERSION);
}

TEST(ApplicationManager, handles_subscribe_eventgroup_accept_and_reject) {
    auto make_subscribe = [] {
        auto message = MessageBuilder::create<SOMEIPSD>();
        message->set_request_id(0x00050001);
        SDEntry entry(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID);
        entry.set_service_id(0x1001);
        entry.set_instance_id(0x0002);
        entry.set_major_version(0x01);
        entry.set_event_group_id(0x0004);
        entry.set_ttl(SOMEIP_DEFAULT_TTL_ON);
        MessageComposer::add_entry(message, &entry);
        return message;
    };

    auto accepted_router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager accepted("test", "", accepted_router);
    accepted.init();
    accepted.offer_service(0x1001, 0x0002, 0x01, 0x00000003);
    accepted.offer_event(0x1001, 0x0002, 0x0003, {0x0004});
    accepted.register_subscription_handler(0x1001, 0x0002, 0x0004, [](std::uint16_t, bool) { return true; });
    accepted.on_subscribe_eventgroup(make_subscribe());

    ASSERT_NE(accepted_router->last_sd_message, nullptr);
    EXPECT_EQ(accepted_router->last_sd_message->entries().front().get_type(), SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    EXPECT_EQ(accepted_router->last_sd_message->entries().front().get_ttl(), SOMEIP_DEFAULT_TTL_ON);

    auto rejected_router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager rejected("test", "", rejected_router);
    rejected.init();
    rejected.offer_service(0x1001, 0x0002, 0x01, 0x00000003);
    rejected.offer_event(0x1001, 0x0002, 0x0003, {0x0004});
    rejected.register_subscription_handler(0x1001, 0x0002, 0x0004, [](std::uint16_t, bool) { return false; });
    rejected.on_subscribe_eventgroup(make_subscribe());

    ASSERT_NE(rejected_router->last_sd_message, nullptr);
    EXPECT_EQ(rejected_router->last_sd_message->entries().front().get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
}

TEST(ApplicationManager, reports_subscription_ack_and_nack_status) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();
    application_manager.request_event(0x1001, 0x0002, 0x0003, {0x0004});
    application_manager.find_service(0x1001, 0x0002, 0x01, 0x00000003);
    application_manager.on_offer_service(
        make_offer_service_message(0x1001, 0x0002, 0x01, 0x00000003, SOMEIP_DEFAULT_TTL_ON));
    application_manager.subscribe(0x1001, 0x0002, 0x0004, 0x01, 0x0003);

    std::vector<std::uint16_t> statuses;
    application_manager.register_subscription_status_handler(
        0x1001, 0x0002, 0x0004, 0x0003,
        [&](std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t status) {
            statuses.push_back(status);
        },
        true);

    auto acknowledgement = MessageBuilder::create<SOMEIPSD>();
    SDEntry entry(SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    entry.set_service_id(0x1001);
    entry.set_instance_id(0x0002);
    entry.set_major_version(0x01);
    entry.set_event_group_id(0x0004);
    entry.set_ttl(SOMEIP_DEFAULT_TTL_ON);
    MessageComposer::add_entry(acknowledgement, &entry);
    application_manager.on_subscribe_eventgroup_ack(acknowledgement);
    ASSERT_EQ(statuses, (std::vector<std::uint16_t>{0}));

    acknowledgement->entry(0).set_ttl(SOMEIP_DEFAULT_TTL_OFF);
    application_manager.on_subscribe_eventgroup_ack(acknowledgement);
    EXPECT_EQ(statuses, (std::vector<std::uint16_t>{0, 0xff}));
}

TEST(ApplicationManager, replays_availability_to_late_specific_and_wildcard_handlers) {
    auto router = std::make_shared<FakeApplicationRouter>();
    ApplicationManager application_manager("test", "", router);
    application_manager.init();
    application_manager.find_service(0x1001, 0x0002, 0x01, 0x00000003);
    application_manager.on_offer_service(
        make_offer_service_message(0x1001, 0x0002, 0x01, 0x00000003, SOMEIP_DEFAULT_TTL_ON));

    struct CallbackState {
        std::mutex mutex;
        std::condition_variable condition;
        int specific_count = 0;
        int wildcard_count = 0;
    };
    auto callback_state = std::make_shared<CallbackState>();
    auto record_callback = [callback_state](int CallbackState::*count, bool available) {
        if (available) {
            std::lock_guard<std::mutex> lock(callback_state->mutex);
            ++(callback_state.get()->*count);
            callback_state->condition.notify_all();
        }
    };

    application_manager.register_availability_handler(
        0x1001, 0x0002,
        [record_callback](std::uint16_t, std::uint16_t, bool available) {
            record_callback(&CallbackState::specific_count, available);
        },
        0x01, 0x00000003);
    application_manager.register_availability_handler(
        0x1001, SOMEIP_DEFAULT_ANY_INSTANCE,
        [record_callback](std::uint16_t, std::uint16_t, bool available) {
            record_callback(&CallbackState::wildcard_count, available);
        },
        0x01, 0x00000003);

    std::unique_lock<std::mutex> lock(callback_state->mutex);
    ASSERT_TRUE(callback_state->condition.wait_for(lock, std::chrono::seconds(1), [&] {
        return callback_state->specific_count == 1 && callback_state->wildcard_count == 1;
    }));
}
