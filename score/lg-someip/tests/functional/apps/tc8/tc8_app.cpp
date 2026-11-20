/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

// Tc8Interface test application for the TC8 conformance suites (test_cases/test_tc8_*.py), on the lgsomeip API.
// Function-aligned with the Tc8Interface DUT the suites were written against:
//   service 0x1234 / instance 0x5678 / major 1
//   Echo 0x0421                      answers any request with an empty response
//   TestField        get 0x0001, set 0x0002, notifier 0x8777 (eventgroups 0x4455 unicast, 0x4465 multicast)
//   TestStaticField  get 0x0003, notifier 0x8779 (eventgroup 0x4480), never changes
//   TestTcpEvent     0x8778 (eventgroup 0x4475)
// TestField and TestTcpEvent are re-notified every 500 ms so the event cases have traffic between sets.

#include <lgsomeip/LgsomeipApi.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

namespace
{

using namespace lgsomeip::api;

constexpr service_t kService = 0x1234;
constexpr instance_t kInstance = 0x5678;
constexpr major_version_t kMajor = 0x01;

constexpr method_t kFieldGet = 0x0001;
constexpr method_t kFieldSet = 0x0002;
constexpr method_t kStaticFieldGet = 0x0003;
constexpr method_t kEcho = 0x0421;

constexpr event_t kFieldNotifier = 0x8777;
constexpr event_t kTcpEvent = 0x8778;
constexpr event_t kStaticFieldNotifier = 0x8779;

constexpr eventgroup_t kFieldGroup = 0x4455;
constexpr eventgroup_t kFieldMulticastGroup = 0x4465;
constexpr eventgroup_t kTcpEventGroup = 0x4475;
constexpr eventgroup_t kStaticFieldGroup = 0x4480;

constexpr std::uint16_t kInitialFieldValue = 0xDEAD;
constexpr std::uint16_t kStaticFieldValue = 0xFEED;
constexpr std::uint16_t kTcpEventValue = 0xBEEF;
constexpr std::chrono::milliseconds kNotifyCycle{500};

std::shared_ptr<Application> app;
std::atomic<std::uint16_t> field_value{kInitialFieldValue};
std::atomic<bool> registered{false};
std::atomic<bool> stop_requested{false};

Payload u16(std::uint16_t value)
{
    return {static_cast<byte_t>(value >> 8), static_cast<byte_t>(value & 0xFF)};
}

void respond(const std::shared_ptr<Message>& request, Payload payload)
{
    if (request->type != MessageType::Request)
    {
        return;  // fire-and-forget and anything else gets no answer
    }
    Message response = *request;
    response.type = MessageType::Response;
    response.return_code = ReturnCode::Ok;
    response.payload = std::move(payload);
    app->send(response);
}

void on_state(bool is_registered)
{
    if (is_registered && !registered.exchange(true))
    {
        app->offer_event(kService, kInstance, kFieldNotifier, {kFieldGroup, kFieldMulticastGroup}, true);
        app->offer_event(kService, kInstance, kStaticFieldNotifier, {kStaticFieldGroup}, true);
        app->offer_event(kService, kInstance, kTcpEvent, {kTcpEventGroup});
        app->offer_service(kService, kInstance, kMajor);
    }
    registered = is_registered;
}

}  // namespace

int main(int argc, char** argv)
{
    std::signal(SIGTERM, [](int) {
        stop_requested = true;
    });
    std::signal(SIGINT, [](int) {
        stop_requested = true;
    });

    app = Runtime::instance().create_application("tc8", argc > 1 ? argv[1] : "");
    if (!app->init())
    {
        std::cerr << "tc8_app: init failed" << std::endl;
        return 1;
    }

    app->register_message_handler(kService, kInstance, kEcho, [](const std::shared_ptr<Message>& request) {
        respond(request, {});
    });
    app->register_message_handler(kService, kInstance, kFieldGet, [](const std::shared_ptr<Message>& request) {
        respond(request, u16(field_value.load()));
    });
    app->register_message_handler(kService, kInstance, kStaticFieldGet, [](const std::shared_ptr<Message>& request) {
        respond(request, u16(kStaticFieldValue));
    });
    app->register_message_handler(kService, kInstance, kFieldSet, [](const std::shared_ptr<Message>& request) {
        if (request->payload.size() >= 2)
        {
            field_value = static_cast<std::uint16_t>((request->payload[0] << 8) | request->payload[1]);
        }
        respond(request, u16(field_value.load()));
        app->notify(kService, kInstance, kFieldNotifier, u16(field_value.load()), true);
    });
    app->register_application_state_handler(on_state);
    app->start();

    bool static_field_seeded = false;
    while (!stop_requested)
    {
        std::this_thread::sleep_for(kNotifyCycle);
        if (registered)
        {
            if (!static_field_seeded)
            {
                // Once, and only after the daemon has processed the offer: a notify that races the offer is dropped
                // ("No Service Information"). Never notified again, so a new subscriber gets this value only.
                app->notify(kService, kInstance, kStaticFieldNotifier, u16(kStaticFieldValue));
                static_field_seeded = true;
            }
            // force: the value is unchanged between sets, and an unforced notify of an unchanged value is not sent.
            app->notify(kService, kInstance, kFieldNotifier, u16(field_value.load()), true);
            app->notify(kService, kInstance, kTcpEvent, u16(kTcpEventValue), true);
        }
    }

    app->stop_offer_service(kService, kInstance);
    app->stop();
    return 0;
}
