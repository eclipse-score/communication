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

// Provider example: offer one service and answer requests for one method.
namespace {

constexpr lgsomeip::api::service_t kServiceId = 0x1001;
constexpr lgsomeip::api::instance_t kInstanceId = 0x0001;
constexpr lgsomeip::api::method_t kMethodId = 0x0001;
constexpr lgsomeip::api::major_version_t kMajorVersion = 0x01;

std::shared_ptr<lgsomeip::api::Application> application;
// Offers are retained by the core and announced again after reconnects.
bool service_offered = false;

void on_message(const std::shared_ptr<lgsomeip::api::Message>& request) {
    if (!request || request->type != lgsomeip::api::MessageType::Request) {
        return;
    }

    // Preserve the request's service, instance, method, client, and session so
    // the requester can correlate this response with its outstanding call.
    lgsomeip::api::Message response;
    response.service = request->service;
    response.instance = request->instance;
    response.method = request->method;
    response.client = request->client;
    response.session = request->session;
    response.interface_version = request->interface_version;
    response.type = lgsomeip::api::MessageType::Response;
    application->send(response);
}

void on_state(bool registered) {
    // Application registration means the daemon connection is ready; it is
    // distinct from offering a service, which this callback does next.
    if (!registered || service_offered) {
        return;
    }

    application->offer_service(kServiceId, kInstanceId, kMajorVersion);
    service_offered = true;
}

} // namespace

int main() {
    application = lgsomeip::api::Runtime::instance().create_application("response");
    if (!application->init()) {
        return 1;
    }

    // Register callbacks before start(); they remain installed across reconnects.
    application->register_message_handler(kServiceId, kInstanceId, kMethodId, on_message, true);
    application->register_application_state_handler(on_state);
    application->start();
    // start() begins processing and returns; join() keeps this provider alive
    // until stop() is called or the process is interrupted.
    application->join();
}
