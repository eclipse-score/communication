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
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <lgsomeip/LgsomeipApi.h>

// Event provider example: offer two events in one event group and publish the
// same four-byte payload on each event once per second.
namespace {

constexpr lgsomeip::api::service_t kServiceId = 0x1001;
constexpr lgsomeip::api::instance_t kInstanceId = 0x0001;
constexpr lgsomeip::api::major_version_t kMajorVersion = 0x01;
// Each offered event must list the event group subscribers will request.
constexpr lgsomeip::api::eventgroup_t kEventGroup = 0x4455;
constexpr lgsomeip::api::event_t kEventId1 = 0x8777;
constexpr lgsomeip::api::event_t kEventId2 = 0x8778;

std::shared_ptr<lgsomeip::api::Application> application;
// The main thread publishes; the state callback updates this flag.
std::atomic<bool> registered{false};
// The core retains event/service offers and restores them after reconnects.
bool service_offered = false;

void on_state(bool is_registered) {
    registered = is_registered;
    if (is_registered && !service_offered) {
        // Define event-group membership before offering the service so clients
        // can discover both events as part of the provider's service.
        const std::set<lgsomeip::api::eventgroup_t> eventgroups{kEventGroup};
        application->offer_event(kServiceId, kInstanceId, kEventId1, eventgroups);
        application->offer_event(kServiceId, kInstanceId, kEventId2, eventgroups);
        application->offer_service(kServiceId, kInstanceId, kMajorVersion);
        service_offered = true;
    }
}

} // namespace

int main() {
    application = lgsomeip::api::Runtime::instance().create_application("notify");
    if (!application->init()) {
        return 1;
    }
    application->register_application_state_handler(on_state);
    application->start();

    // The API transports bytes, not application structs. Encode/decode any
    // application-specific fields before passing the payload to notify().
    lgsomeip::api::Payload payload{'D', 'A', 'T', 'A'};
    while (true) {
        if (registered) {
            // Change one byte so subscribers can see each published update.
            payload[0] = payload[0] == 'D' ? 'F' : 'D';
            application->notify(kServiceId, kInstanceId, kEventId1, payload);
            application->notify(kServiceId, kInstanceId, kEventId2, payload);
            std::cout << "notifications sent" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
