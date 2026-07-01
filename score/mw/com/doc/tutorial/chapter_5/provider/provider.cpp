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
#include "score/mw/com/doc/tutorial/chapter_5/hello_world_service.h"
#include "score/mw/com/types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <thread>

// In this chapter the provider offers a single instance of the HelloWorldService.
constexpr std::string_view kInstanceSpecifierString{"MyHelloWorldServiceInstance"};

// In contrast to the previous chapters, the provider does NOT send its event updates strictly cyclically. Instead it
// waits a randomized amount of time (between kMinSendDelay and kMaxSendDelay) before it sends the next sample. This
// models a provider that emits event updates only occasionally / at times not anticipated by the consumer - which is
// exactly the scenario in which an event-driven (receive-handler based) consumer shines.
constexpr std::chrono::milliseconds kMinSendDelay{100};
constexpr std::chrono::milliseconds kMaxSendDelay{3000};

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

    auto offer_result = service_instance.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(),
                                                "Failed to offer HelloWorldSkeleton instance!");
    std::cout << "OfferService: HelloWorld service instance is now offered." << std::endl;

    // Random number generator for the randomized send delay.
    std::mt19937 random_engine{std::random_device{}()};
    std::uniform_int_distribution<std::chrono::milliseconds::rep> delay_distribution{kMinSendDelay.count(),
                                                                                     kMaxSendDelay.count()};

    std::size_t send_counter{0};
    while (g_running.load(std::memory_order_relaxed))
    {
        // Wait a randomized amount of time before sending the next event update.
        const std::chrono::milliseconds send_delay{delay_distribution(random_engine)};
        std::cout << "Next event update in " << send_delay.count() << " ms." << std::endl;
        std::this_thread::sleep_for(send_delay);

        if (!g_running.load(std::memory_order_relaxed))
        {
            break;
        }

        SendSample(service_instance, send_counter);
        ++send_counter;
    }
    std::cout << "HelloWorld service provider going down." << std::endl;
}
