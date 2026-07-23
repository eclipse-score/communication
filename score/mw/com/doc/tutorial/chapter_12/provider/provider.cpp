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
#include "score/mw/com/doc/tutorial/chapter_12/provider/skeleton_component.h"
#include "score/mw/com/types.h"

#include <atomic>
#include <csignal>
#include <cstdlib>

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    std::cout << "HelloWorld service provider caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{"MyHelloWorldServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    auto hello_world_service_instance_result =
        score::mw::com::tutorial::HelloWorldSkeleton::Create(instance_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(hello_world_service_instance_result.has_value(),
                                                "Failed to create HelloWorldSkeleton instance!");
    auto hello_world_service_instance = std::move(hello_world_service_instance_result).value();

    score::mw::com::tutorial::SkeletonComponent skeleton_component{std::move(hello_world_service_instance)};
    if (!skeleton_component.OfferService())
    {
        exit(1);
    }

    size_t send_counter{0};
    while (g_running.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto send_sample_result = skeleton_component.SendSample(send_counter);
        if (send_sample_result == score::mw::com::tutorial::SkeletonComponent::SendSampleResult::kNoSampleAllocated)
        {
            continue;
        }
        if (send_sample_result == score::mw::com::tutorial::SkeletonComponent::SendSampleResult::kFatalError)
        {
            exit(1);
        }
        ++send_counter;
    }
    std::cout << "HelloWorld service provider going down." << std::endl;
}
