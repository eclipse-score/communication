/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/generic_skeleton.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/traits.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/log/logging.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

// Default to 64-byte if not specified by the build system
#ifndef PAYLOAD_SIZE
#define PAYLOAD_SIZE 64
#endif

namespace
{

struct MyEventData
{
    uint64_t counter;
#if PAYLOAD_SIZE > 8
    char padding[PAYLOAD_SIZE - 8];
#endif
};

constexpr std::string_view kInstanceSpecifier = "/test/generic/typed/interaction";
constexpr int kSamplesToProcess = 30;
constexpr int kSamplesToSubscribe = 5;

#if PAYLOAD_SIZE == 64
constexpr std::string_view kEventName = "Event64Byte";
#elif PAYLOAD_SIZE == 32
constexpr std::string_view kEventName = "Event32Byte";
#elif PAYLOAD_SIZE == 16
constexpr std::string_view kEventName = "Event16Byte";
#elif PAYLOAD_SIZE == 8
constexpr std::string_view kEventName = "Event8Byte";
#else
#error "Unsupported payload size configured."
#endif

int run_provider()
{
    const auto instance_specifier =
        score::mw::com::impl::InstanceSpecifier::Create(std::string{kInstanceSpecifier}).value();
    const score::mw::com::impl::DataTypeMetaInfo meta{sizeof(MyEventData), alignof(MyEventData)};
    const std::vector<score::mw::com::impl::EventInfo> events = {{kEventName, meta}};

    score::mw::com::impl::GenericSkeletonServiceElementInfo create_params;
    create_params.events = events;

    auto skeleton_res = score::mw::com::impl::GenericSkeleton::Create(instance_specifier, create_params);
    if (!skeleton_res.has_value())
    {
        score::mw::log::LogFatal("GenericSkeletonProvider") << "Failed to create skeleton.";
        return 1;
    }
    auto& skeleton = skeleton_res.value();

    if (!skeleton.OfferService().has_value())
    {
        score::mw::log::LogFatal("GenericSkeletonProvider") << "Failed to offer service.";
        return 1;
    }

    auto it = skeleton.GetEvents().find(kEventName);
    if (it == skeleton.GetEvents().cend())
    {
        score::mw::log::LogFatal("GenericSkeletonProvider") << "Failed to find event in skeleton.";
        return 1;
    }
    auto& generic_event = const_cast<score::mw::com::impl::GenericSkeletonEvent&>(it->second);

    // Wait for the consumer to start and subscribe BEFORE sending data
    score::mw::log::LogInfo("GenericSkeletonProvider")
        << PAYLOAD_SIZE << "-byte - Waiting 5s for consumer to subscribe...";
    std::this_thread::sleep_for(std::chrono::seconds(5));

    for (int i = 0; i < kSamplesToProcess; ++i)
    {
        auto sample_res = generic_event.Allocate();
        if (!sample_res.has_value())
        {
            score::mw::log::LogFatal("GenericSkeletonProvider") << "Failed to allocate sample.";
            return 1;
        }
        auto* typed_sample = static_cast<MyEventData*>(sample_res.value().Get());
        typed_sample->counter = i;
        generic_event.Send(std::move(sample_res.value()));

        score::mw::log::LogInfo("GenericSkeletonProvider") << PAYLOAD_SIZE << "-byte Event Sent sample: " << i;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::seconds(15));
    skeleton.StopOfferService();
    return 0;
}

template <typename Trait>
class MyTestService : public Trait::Base
{
  public:
    using Trait::Base::Base;
    typename Trait::template Event<MyEventData> event_{*this, std::string{kEventName}};
};
using MyTestServiceProxy = score::mw::com::impl::AsProxy<MyTestService>;

int run_consumer()
{
    const auto instance_specifier =
        score::mw::com::impl::InstanceSpecifier::Create(std::string{kInstanceSpecifier}).value();

    score::Result<score::mw::com::ServiceHandleContainer<MyTestServiceProxy::HandleType>> handles_res;
    int retries = 0;
    while (retries < 50)  // Try for up to 5 seconds
    {
        handles_res = MyTestServiceProxy::FindService(instance_specifier);
        if (handles_res.has_value() && !handles_res.value().empty())
        {
            break;  // Service found!
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries++;
    }

    if (!handles_res.has_value() || handles_res.value().empty())
    {
        score::mw::log::LogFatal("TypedProxyConsumer") << "Failed to find service after waiting.";
        return 1;
    }

    auto proxy_res = MyTestServiceProxy::Create(handles_res.value()[0]);
    if (!proxy_res.has_value())
    {
        score::mw::log::LogFatal("TypedProxyConsumer") << "Failed to create proxy.";
        return 1;
    }
    auto& proxy = proxy_res.value();

    uint64_t received = 0;
    uint64_t expected = 0;
    int data_mismatches = 0;
    proxy.event_.Subscribe(kSamplesToSubscribe);

    while (received < kSamplesToProcess)
    {
        proxy.event_.GetNewSamples(
            [&](score::mw::com::SamplePtr<MyEventData> sample) {
                if (sample->counter != expected)
                {
                    score::mw::log::LogFatal("TypedProxyConsumer")
                        << PAYLOAD_SIZE << "-byte data failed! Exp " << expected << ", got " << sample->counter;
                    data_mismatches++;
                }
                else
                {
                    score::mw::log::LogInfo("TypedProxyConsumer")
                        << "Received " << PAYLOAD_SIZE << "-byte sample: " << sample->counter;
                }
                expected++;
                received++;
            },
            kSamplesToSubscribe);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (data_mismatches > 0)
    {
        score::mw::log::LogFatal("TypedProxyConsumer") << "Test failed with " << data_mismatches << " mismatches.";
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, const char* argv[])
{
    std::string mode;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc)
        {
            mode = argv[++i];
        }
    }

    score::mw::com::runtime::InitializeRuntime(score::mw::com::runtime::RuntimeConfiguration(argc, argv));

    if (mode == "provider")
    {
        return run_provider();
    }
    else if (mode == "consumer")
    {
        return run_consumer();
    }

    score::mw::log::LogFatal("Main") << "Invalid or missing mode. Use --mode provider or --mode consumer.";
    return 1;
}
