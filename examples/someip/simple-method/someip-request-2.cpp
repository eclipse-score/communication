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
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include <lgsomeip/LgsomeipApi.h>

// Latency example: send 100,000 requests and report every thousandth response.
namespace {

constexpr lgsomeip::api::service_t kServiceId = 0x1001;
constexpr lgsomeip::api::instance_t kInstanceId = 0x0001;
constexpr lgsomeip::api::method_t kMethodId = 0x0001;
constexpr lgsomeip::api::major_version_t kMajorVersion = 0x01;

using Clock = std::chrono::steady_clock;
std::shared_ptr<lgsomeip::api::Application> application;
// Key timings by session so responses can be matched to their requests.
std::map<std::uint16_t, Clock::time_point> request_start_times;
// The request loop and message callbacks run on different threads.
std::mutex request_times_mutex;
std::atomic<std::uint16_t> next_session{0};
std::atomic<std::uint32_t> response_count{0};
std::atomic<bool> service_available{false};
bool service_requested = false;

void on_message(const std::shared_ptr<lgsomeip::api::Message>& message) {
    Clock::time_point start_time;
    {
        // The callback runs on a worker thread, so protect the timing map shared
        // with the request-producing main thread.
        std::lock_guard<std::mutex> lock(request_times_mutex);
        const auto found = request_start_times.find(message->session);
        if (found == request_start_times.end()) {
            return;
        }
        start_time = found->second;
        request_start_times.erase(found);
    }

    const auto count = ++response_count;
    if (count % 1000 == 0) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start_time);
        std::cout << application->name() << " response " << count << " latency=" << elapsed.count() << " us"
                  << std::endl;
    }
}

void on_availability(lgsomeip::api::service_t, lgsomeip::api::instance_t, bool available) {
    service_available = available;
}

void on_state(bool registered) {
    // Register the service request once; the core retries this stored request
    // after reconnects. Availability is tracked separately below.
    if (registered && !service_requested) {
        application->request_service(kServiceId, kInstanceId, kMajorVersion);
        service_requested = true;
    }
}

} // namespace

int main() {
    application = lgsomeip::api::Runtime::instance().create_application("request-2");
    if (!application->init()) {
        return 1;
    }

    application->register_message_handler(kServiceId, kInstanceId, kMethodId, on_message);
    application->register_availability_handler(kServiceId, kInstanceId, on_availability, kMajorVersion);
    application->register_application_state_handler(on_state);
    application->start();
    // Do not send until discovery reports the provider is available.
    while (!service_available) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    constexpr std::uint32_t kRequestCount = 100000;
    for (std::uint32_t sent = 0; sent < kRequestCount; ++sent) {
        // SOME/IP's nonzero 16-bit session ID correlates this response with
        // its request; it also indexes the local latency timestamp.
        auto session = static_cast<std::uint16_t>(++next_session);
        if (session == 0) {
            session = static_cast<std::uint16_t>(++next_session);
        }

        lgsomeip::api::Message request;
        request.service = kServiceId;
        request.instance = kInstanceId;
        request.method = kMethodId;
        request.session = session;
        request.interface_version = kMajorVersion;
        request.type = lgsomeip::api::MessageType::Request;
        // Insert before send() so a fast response callback always finds its timestamp.
        {
            std::lock_guard<std::mutex> lock(request_times_mutex);
            request_start_times[session] = Clock::now();
        }
        application->send(request);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << application->name() << " sent=" << kRequestCount << " received=" << response_count << std::endl;
    application->stop();
    application->join();

}
