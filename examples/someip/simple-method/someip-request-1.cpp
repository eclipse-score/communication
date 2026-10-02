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

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include <lgsomeip/LgsomeipApi.h>

// Consumer example: discover a service, then send one request per second while
// the provider is available.
namespace {

constexpr lgsomeip::api::service_t kServiceId = 0x1001;
constexpr lgsomeip::api::instance_t kInstanceId = 0x0001;
constexpr lgsomeip::api::method_t kMethodId = 0x0001;
constexpr lgsomeip::api::major_version_t kMajorVersion = 0x01;

std::shared_ptr<lgsomeip::api::Application> application;
std::atomic<bool> service_available{false};
// The core remembers requested services and retries discovery after reconnects.
bool service_requested = false;

void on_message(const std::shared_ptr<lgsomeip::api::Message>& message) {
    std::cout << application->name() << " response: service=0x" << std::hex << message->service << " method=0x"
              << message->method << std::dec << std::endl;
}

void on_availability(lgsomeip::api::service_t, lgsomeip::api::instance_t, bool available) {
    std::cout << application->name() << " service available = " << available << std::endl;
    // Availability is separate from application registration. This callback
    // can run on a worker thread while main sends requests.
    service_available = available;
}

void on_state(bool registered) {
    // Request discovery once. The core retains the request and retries it
    // after reconnects, so repeated registration must not add duplicate state.
    if (registered && !service_requested) {
        application->request_service(kServiceId, kInstanceId, kMajorVersion);
        service_requested = true;
    }
}

} // namespace

int main() {
    application = lgsomeip::api::Runtime::instance().create_application("request-1");
    if (!application->init()) {
        return 1;
    }

    application->register_application_state_handler(on_state);
    // Install callbacks before start() so the first response or availability
    // transition cannot arrive before its handler is registered.
    application->register_message_handler(kServiceId, kInstanceId, kMethodId, on_message);
    application->register_availability_handler(kServiceId, kInstanceId, on_availability, kMajorVersion);
    application->start();

    while (true) {
        if (service_available) {
            // A request is identified by its service/instance/method and
            // interface version. The core assigns the client/session ID on send.
            lgsomeip::api::Message request;
            request.service = kServiceId;
            request.instance = kInstanceId;
            request.method = kMethodId;
            request.interface_version = kMajorVersion;
            request.type = lgsomeip::api::MessageType::Request;
            application->send(request);
        }
        // Back off between calls; service_available gates sends while the
        // service is absent without stopping the application's receive loop.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
