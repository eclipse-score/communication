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
#include "../hello_world_service.h"
#include "score/mw/com/types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>

// In this chapter the provider offers a single instance of the HelloWorldService.
constexpr std::string_view kInstanceSpecifierString{"MyHelloWorldServiceInstance"};

// The provider offers the service and sends event updates for kOfferDuration. Afterwards it stops offering the service
// (StopOfferService) and pauses sending for kStopOfferDuration. Then it offers the service again and resumes sending.
// This offer/stop-offer cycle drives the subscription state of subscribed consumers between kSubscribed and
// kSubscriptionPending.
constexpr std::chrono::seconds kOfferDuration{8};
constexpr std::chrono::seconds kStopOfferDuration{5};

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    std::cout << "HelloWorld service provider caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

using HelloWorldSkeleton = score::mw::com::AsSkeleton<score::mw::com::tutorial::HelloWorldInterface>;

// Publishes one "message" event sample on the given service instance.
static void SendSample(HelloWorldSkeleton& service_instance, const std::size_t send_counter)
{
    // Allocate a slot, where to publish the next event
    auto sample_allocatee_ptr = service_instance.message.Allocate();
    if (!sample_allocatee_ptr)
    {
        std::cerr << "Failed to allocate sample_allocatee_ptr: " << sample_allocatee_ptr.error() << std::endl;
        return;
    }

    // Write new event data to the slot.
    const std::string message = std::string{"Hello World #"} + std::to_string(send_counter);
    auto* const buf = sample_allocatee_ptr.value().Get()->data();
    const auto capacity = sample_allocatee_ptr.value().Get()->size();
    const auto chars_to_copy = std::min(message.size(), capacity - 1U);
    std::memcpy(buf, message.data(), chars_to_copy);
    buf[chars_to_copy] = '\0';

    // Send the new event sample (make it visible to potential consumers)
    auto send_result = service_instance.message.Send(std::move(sample_allocatee_ptr.value()));
    if (!send_result)
    {
        std::cerr << "Failed to send sample_allocatee_ptr: " << send_result.error() << std::endl;
    }
    else
    {
        std::cout << "Sample send completed. Event \"message\" update sent: " << buf << std::endl;
    }
}

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    auto service_instance_result = HelloWorldSkeleton::Create(instance_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_instance_result.has_value(),
                                                "Failed to create HelloWorldSkeleton instance!");
    auto service_instance = std::move(service_instance_result).value();

    // Bring the service instance up initially.
    auto offer_result = service_instance.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(),
                                                "Failed to offer HelloWorldSkeleton instance!");
    std::cout << "OfferService: HelloWorld service instance is now offered." << std::endl;

    bool offered{true};
    auto next_phase_change = std::chrono::steady_clock::now() + kOfferDuration;

    std::size_t send_counter{0};
    while (g_running.load(std::memory_order_relaxed))
    {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_phase_change)
        {
            if (offered)
            {
                // Stop offering the service. Consumers that are subscribed to the "message" event will observe their
                // subscription state transition from kSubscribed to kSubscriptionPending.
                service_instance.StopOfferService();
                offered = false;
                next_phase_change = now + kStopOfferDuration;
                std::cout
                    << "StopOfferService: HelloWorld service instance is no longer offered. Pausing event updates."
                    << std::endl;
            }
            else
            {
                // Offer the service again. Subscribed consumers will observe their subscription state transition back
                // from kSubscriptionPending to kSubscribed.
                auto reoffer_result = service_instance.OfferService();
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(reoffer_result.has_value(),
                                                            "Failed to re-offer HelloWorldSkeleton instance!");
                offered = true;
                next_phase_change = now + kOfferDuration;
                std::cout << "OfferService: HelloWorld service instance is offered again. Resuming event updates."
                          << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Only send event updates while the service is currently offered.
        if (offered)
        {
            SendSample(service_instance, send_counter);
            ++send_counter;
        }
    }
    std::cout << "HelloWorld service provider going down." << std::endl;
}
