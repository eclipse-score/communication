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
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>

#include <lgsomeip/LgsomeipApi.h>

// Negative test: method 0x0002 is not offered by the response example, so the
// client expects the daemon/provider path to return a SOME/IP error response.
namespace {

constexpr lgsomeip::api::service_t kServiceId = 0x1001;
constexpr lgsomeip::api::instance_t kInstanceId = 0x0001;
// This method is intentionally not offered by the response sample.
constexpr lgsomeip::api::method_t kMethodId = 0x0002;

bool response_done = false;
std::condition_variable condition_var;
std::mutex method_mutex;
std::shared_ptr<lgsomeip::api::Application> application;
bool service_requested = false;
std::atomic<bool> request_sent{false};

void on_message(const std::shared_ptr<lgsomeip::api::Message>&) {
    std::cout << "[Wrong Method Request Test] Response was arrived even though wrong method id used. What happens?"
              << std::endl;

    {
        std::lock_guard<std::mutex> lock(method_mutex);
        response_done = true;
    }
    condition_var.notify_one();
}

void on_availability(lgsomeip::api::service_t, lgsomeip::api::instance_t, bool available) {
    if (available && !request_sent.exchange(true)) {
        lgsomeip::api::Message request;
        request.service = kServiceId;
        request.instance = kInstanceId;
        request.method = kMethodId;
        request.session = 1;
        request.type = lgsomeip::api::MessageType::Request;
        application->send(request);
    }
}

void on_state(bool registered) {
    if (registered && !service_requested) {
        application->request_service(kServiceId, kInstanceId);
        service_requested = true;
    }
}

} // namespace

int main() {
    application = lgsomeip::api::Runtime::instance().create_application("wrongmethod-test");
    if (!application->init()) {
        return 1;
    }
    application->register_message_handler(kServiceId, kInstanceId, kMethodId, on_message);
    application->register_availability_handler(kServiceId, kInstanceId, on_availability);
    application->register_application_state_handler(on_state);
    application->start();

    // The message callback signals this condition when the error response arrives.
    {
        std::unique_lock<std::mutex> lock(method_mutex);
        if (!condition_var.wait_for(lock, std::chrono::seconds(10), [] { return response_done; })) {
            std::cerr << "[Wrong Method Request Test] Timed out waiting for the error response" << std::endl;
            application->stop();
            application->join();
            return 1;
        }
    }

    std::cout << "[Wrong Method Request Test] Done" << std::endl;
    application->stop();
    application->join();
}
