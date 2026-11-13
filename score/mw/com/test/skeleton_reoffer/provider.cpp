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
#include "score/mw/com/test/skeleton_reoffer/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/skeleton_reoffer/test_datatype.h"
#include "score/mw/com/test/skeleton_reoffer/test_parameters.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace score::mw::com::test::skeleton_reoffer
{
namespace
{

constexpr std::string_view kTestNamePrefix{"skeleton_reoffer"};
constexpr std::chrono::milliseconds kSampleSendInterval{20};

void SendSamples(TestSkeleton& skeleton, const std::size_t num_samples, const std::uint32_t initial_value)
{
    std::cout << "Provider: Sending " << num_samples << " samples starting at " << initial_value << std::endl;
    for (std::size_t i = 0U; i < num_samples; ++i)
    {
        const auto sample_value = initial_value + static_cast<std::uint32_t>(i);
        const auto send_result = skeleton.test_event.Send(sample_value);
        if (!send_result.has_value())
        {
            FailTest("skeleton_reoffer provider: Send failed: ", send_result.error());
        }
        std::this_thread::sleep_for(kSampleSendInterval);
    }
}

}  // namespace

void run_provider(const Scenario scenario, const score::cpp::stop_token& stop_token)
{
    auto first_batch_received_sync = CreateSynchronizer(kFirstBatchReceivedShmPath);
    auto stop_offer_done_sync = CreateSynchronizer(kStopOfferDoneShmPath);
    auto ready_for_reoffer_sync = CreateSynchronizer(kReadyForReofferShmPath);
    auto reoffer_done_sync = CreateSynchronizer(kReofferDoneShmPath);
    auto consumer_done_sync = CreateSynchronizer(kConsumerDoneShmPath);

    ExitFunctionGuard done_guard{[&stop_offer_done_sync, &reoffer_done_sync]() {
        stop_offer_done_sync->Notify();
        reoffer_done_sync->Notify();
    }};

    SkeletonContainer<TestSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(GetTestInstanceSpecifier(), std::string{kTestNamePrefix});
    auto& skeleton = skeleton_container.GetSkeleton();

    skeleton_container.OfferService(std::string{kTestNamePrefix});
    SendSamples(skeleton, kNumSamplesPerOffer, kFirstOfferInitialValue);

    if (scenario != Scenario::kNoConnectedProxy)
    {
        WaitForNotification(first_batch_received_sync, stop_token, "first_batch_received");
    }

    skeleton.StopOfferService();

    if (scenario != Scenario::kNoConnectedProxy)
    {
        stop_offer_done_sync->Notify();
        WaitForNotification(ready_for_reoffer_sync, stop_token, "ready_for_reoffer");
    }

    skeleton_container.OfferService(std::string{kTestNamePrefix});
    reoffer_done_sync->Notify();

    SendSamples(skeleton, kNumSamplesPerOffer, kSecondOfferInitialValue);
    WaitForNotification(consumer_done_sync, stop_token, "consumer_done");
}

}  // namespace score::mw::com::test::skeleton_reoffer
