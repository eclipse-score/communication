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
#include "score/mw/com/doc/tutorial/chapter_10/hello_world_service.h"
#include "score/mw/com/types.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>

constexpr std::string_view kHelloWorld{"Hello World"};
static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    std::cout << "HelloWorld service provider caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

using HelloWorldSkeleton = score::mw::com::AsSkeleton<score::mw::com::tutorial::HelloWorldInterface>;

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyHelloWorldServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    auto hello_world_service_instance_result = HelloWorldSkeleton::Create(instance_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(hello_world_service_instance_result.has_value(),
                                                "Failed to create HelloWorldSkeleton instance!");
    auto hello_world_service_instance = std::move(hello_world_service_instance_result).value();

    auto offer_result = hello_world_service_instance.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(),
                                                "Failed to offer HelloWorldSkeleton instance!");

    size_t send_counter{0};
    while (g_running.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Allocate a slot, where to publish the next event
        auto sample_allocatee_ptr = hello_world_service_instance.message.Allocate();
        if (!sample_allocatee_ptr)
        {
            std::cerr << "Failed to allocate sample_allocatee_ptr: " << sample_allocatee_ptr.error() << std::endl;
            continue;
        }

        // Write new event data to slot
        std::memcpy(sample_allocatee_ptr.value().Get()->data(), kHelloWorld.data(), kHelloWorld.size());
        auto* buf = sample_allocatee_ptr.value().Get()->data();
        const auto remaining = sample_allocatee_ptr.value().Get()->size() - kHelloWorld.size();
        const auto chars_written = std::snprintf(buf + kHelloWorld.size(), remaining, "%zu", send_counter);
        if (chars_written < 0)
        {
            std::cerr << "Failed to write 'send_counter' to sample_allocatee_ptr!" << std::endl;
            exit(1);
        }

        // Send the new event sample (make it visible to potential consumers)
        auto send_result = hello_world_service_instance.message.Send(std::move(sample_allocatee_ptr.value()));
        if (!send_result)
        {
            std::cerr << "Failed to send sample_allocatee_ptr: " << send_result.error() << std::endl;
        }
        else
        {
            std::cout << "Sample send completed. Event \"message\" update sent: " << buf << std::endl;
        }
        ++send_counter;
    }
    std::cout << "HelloWorld service provider going down." << std::endl;
}
