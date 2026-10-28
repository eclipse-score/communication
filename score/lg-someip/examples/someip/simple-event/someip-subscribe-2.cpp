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

#include <iostream>
#include <memory>

#include <lgsomeip/LgsomeipApi.h>

// Second event consumer: it uses the same event-group flow as subscribe-1 but
// reports payload size instead of interpreting the bytes.
namespace {

constexpr lgsomeip::api::service_t kServiceId = 0x1001;
constexpr lgsomeip::api::instance_t kInstanceId = 0x0001;
constexpr lgsomeip::api::major_version_t kMajorVersion = 0x01;
// The provider offers both events in this group.
constexpr lgsomeip::api::eventgroup_t kEventGroup = 0x4455;
constexpr lgsomeip::api::event_t kEventId1 = 0x8777;
constexpr lgsomeip::api::event_t kEventId2 = 0x8778;

std::shared_ptr<lgsomeip::api::Application> application;
bool subscriptions_requested = false;

void on_message(const std::shared_ptr<lgsomeip::api::Message>& message) {
    std::cout << "subscribe-2 / service=0x" << std::hex << message->service << " event=0x" << message->method
              << std::dec << " / payload size=" << message->payload.size() << std::endl;
}

void on_availability(lgsomeip::api::service_t, lgsomeip::api::instance_t, bool available) {
    std::cout << "subscribe-2 / service available = " << available << std::endl;
    if (available) {
        // Events are requested by ID, then received after subscribing by group.
        application->subscribe(kServiceId, kInstanceId, kEventGroup, kMajorVersion);
    } else {
        application->unsubscribe(kServiceId, kInstanceId, kEventGroup);
    }
}

void on_state(bool registered) {
    // Register interests once. The core retains these relationships and
    // restores discovery/subscription behavior after reconnects.
    if (registered && !subscriptions_requested) {
        application->request_event(kServiceId, kInstanceId, kEventId1, {kEventGroup});
        application->request_event(kServiceId, kInstanceId, kEventId2, {kEventGroup});
        application->request_service(kServiceId, kInstanceId, kMajorVersion);
        subscriptions_requested = true;
    }
}

} // namespace

int main() {
    application = lgsomeip::api::Runtime::instance().create_application("subscribe-2");
    if (!application->init()) {
        return 1;
    }

    // Install notification handlers before start() begins receiving messages.
    application->register_message_handler(kServiceId, kInstanceId, kEventId1, on_message);
    application->register_message_handler(kServiceId, kInstanceId, kEventId2, on_message);
    application->register_availability_handler(kServiceId, kInstanceId, on_availability, kMajorVersion);
    application->register_application_state_handler(on_state);
    application->start();
    application->join();
}
