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

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// In this chapter the provider offers 3 different instances of the HelloWorldService. Each instance is mapped to its
// own InstanceSpecifier in the provider configuration (mw_com_config.json).
constexpr std::array<std::string_view, 3U> kInstanceSpecifierStrings{"MyHelloWorldServiceInstance_1",
                                                                     "MyHelloWorldServiceInstance_2",
                                                                     "MyHelloWorldServiceInstance_3"};

// The provider waits this amount of time after bringing up a service instance before it brings up the next one.
constexpr std::chrono::seconds kInstanceStaggerDelay{5};

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    std::cout << "HelloWorld service provider caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

using HelloWorldSkeleton = score::mw::com::AsSkeleton<score::mw::com::tutorial::HelloWorldInterface>;

// Creates a HelloWorldService skeleton for the given InstanceSpecifier and offers it.
static HelloWorldSkeleton CreateAndOfferInstance(const std::string_view instance_specifier_string)
{
    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{instance_specifier_string});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    auto hello_world_service_instance_result = HelloWorldSkeleton::Create(instance_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(hello_world_service_instance_result.has_value(),
                                                "Failed to create HelloWorldSkeleton instance!");
    auto hello_world_service_instance = std::move(hello_world_service_instance_result).value();

    auto offer_result = hello_world_service_instance.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(),
                                                "Failed to offer HelloWorldSkeleton instance!");

    return hello_world_service_instance;
}

// Publishes one "message" event sample on the given service instance.
static void SendSample(HelloWorldSkeleton& service_instance,
                       const std::string_view instance_specifier_string,
                       const std::size_t send_counter)
{
    // Allocate a slot, where to publish the next event
    auto sample_allocatee_ptr = service_instance.message.Allocate();
    if (!sample_allocatee_ptr)
    {
        std::cerr << "Failed to allocate sample_allocatee_ptr: " << sample_allocatee_ptr.error() << std::endl;
        return;
    }

    // Write new event data to the slot. We include the instance specifier so that a consumer can distinguish, from
    // which service instance a received sample originates.
    const std::string message =
        std::string{"Hello World from "} + std::string{instance_specifier_string} + " #" + std::to_string(send_counter);
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

    std::vector<HelloWorldSkeleton> service_instances{};
    service_instances.reserve(kInstanceSpecifierStrings.size());

    std::size_t next_instance_index{0};
    auto next_instance_time = std::chrono::steady_clock::now();

    std::size_t send_counter{0};
    while (g_running.load(std::memory_order_relaxed))
    {
        // Bring up the next service instance once its scheduled time has been reached. This staggers the creation of
        // the 3 service instances by kInstanceStaggerDelay each.
        if ((next_instance_index < kInstanceSpecifierStrings.size()) &&
            (std::chrono::steady_clock::now() >= next_instance_time))
        {
            const auto instance_specifier_string = kInstanceSpecifierStrings[next_instance_index];
            service_instances.push_back(CreateAndOfferInstance(instance_specifier_string));
            std::cout << "Created and offered HelloWorld service instance: " << instance_specifier_string << std::endl;
            ++next_instance_index;
            next_instance_time = std::chrono::steady_clock::now() + kInstanceStaggerDelay;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Send a new event sample on every service instance that is already up and running.
        for (std::size_t i = 0; i < service_instances.size(); ++i)
        {
            SendSample(service_instances[i], kInstanceSpecifierStrings[i], send_counter);
        }
        ++send_counter;
    }
    std::cout << "HelloWorld service provider going down." << std::endl;
}
