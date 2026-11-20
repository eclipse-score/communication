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
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace score::mw::com::test::skeleton_reoffer
{
namespace
{

constexpr std::string_view kTestNamePrefix{"skeleton_reoffer"};
constexpr std::chrono::milliseconds kSampleSendInterval{20};

void SendSamples(const std::vector<std::uint32_t>& samples, TestSkeleton& skeleton)
{
    std::cout << "Provider: Sending " << samples.size() << " samples" << std::endl;
    for (const auto sample_value : samples)
    {
        const auto send_result = skeleton.test_event.Send(sample_value);
        if (!send_result.has_value())
        {
            FailTest("skeleton_reoffer provider: Send failed: ", send_result.error());
        }
        std::this_thread::sleep_for(kSampleSendInterval);
    }
}

void run_provider_no_connected_proxy(const score::cpp::stop_token& stop_token)
{
    auto reoffer_done_sync = ProcessSynchronizer::Create(std::string{kReofferDoneShmPath});
    auto second_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kSecondBatchOfSamplesReceivedShmPath});

    ExitFunctionGuard done_guard{[&reoffer_done_sync]() {
        reoffer_done_sync->Notify();
    }};

    const auto first_offer_samples = CreateExpectedSamples(kFirstOfferInitialValue, kNumSamplesPerOffer);
    const auto second_offer_samples = CreateExpectedSamples(kSecondOfferInitialValue, kNumSamplesPerOffer);

    SkeletonContainer<TestSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(CreateTestInstanceSpecifierOrFail(), std::string{kTestNamePrefix});
    auto& skeleton = skeleton_container.GetSkeleton();

    skeleton_container.OfferService(std::string{kTestNamePrefix});
    SendSamples(first_offer_samples, skeleton);

    skeleton.StopOfferService();
    skeleton_container.OfferService(std::string{kTestNamePrefix});
    reoffer_done_sync->Notify();

    SendSamples(second_offer_samples, skeleton);
    WaitForNotificationOrFail(second_batch_of_samples_received_sync, stop_token, "second_batch_of_samples_received");
}

void run_provider_with_connected_proxy(const score::cpp::stop_token& stop_token)
{
    auto first_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kFirstBatchOfSamplesReceivedShmPath});
    auto second_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kSecondBatchOfSamplesReceivedShmPath});

    const auto first_offer_samples = CreateExpectedSamples(kFirstOfferInitialValue, kNumSamplesPerOffer);
    const auto second_offer_samples = CreateExpectedSamples(kSecondOfferInitialValue, kNumSamplesPerOffer);

    SkeletonContainer<TestSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(CreateTestInstanceSpecifierOrFail(), std::string{kTestNamePrefix});
    auto& skeleton = skeleton_container.GetSkeleton();

    skeleton_container.OfferService(std::string{kTestNamePrefix});
    SendSamples(first_offer_samples, skeleton);
    WaitForNotificationOrFail(first_batch_of_samples_received_sync, stop_token, "first_batch_of_samples_received");

    skeleton.StopOfferService();
    skeleton_container.OfferService(std::string{kTestNamePrefix});

    SendSamples(second_offer_samples, skeleton);
    WaitForNotificationOrFail(second_batch_of_samples_received_sync, stop_token, "second_batch_of_samples_received");
}

void run_provider_proxy_disconnected_before_reoffer(const score::cpp::stop_token& stop_token)
{
    auto first_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kFirstBatchOfSamplesReceivedShmPath});
    auto stop_offer_done_sync = ProcessSynchronizer::Create(std::string{kStopOfferDoneShmPath});
    auto ready_for_reoffer_sync = ProcessSynchronizer::Create(std::string{kReadyForReofferShmPath});
    auto reoffer_done_sync = ProcessSynchronizer::Create(std::string{kReofferDoneShmPath});
    auto second_batch_of_samples_received_sync =
        ProcessSynchronizer::Create(std::string{kSecondBatchOfSamplesReceivedShmPath});

    ExitFunctionGuard done_guard{[&stop_offer_done_sync, &reoffer_done_sync]() {
        stop_offer_done_sync->Notify();
        reoffer_done_sync->Notify();
    }};

    const auto first_offer_samples = CreateExpectedSamples(kFirstOfferInitialValue, kNumSamplesPerOffer);
    const auto second_offer_samples = CreateExpectedSamples(kSecondOfferInitialValue, kNumSamplesPerOffer);

    SkeletonContainer<TestSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(CreateTestInstanceSpecifierOrFail(), std::string{kTestNamePrefix});
    auto& skeleton = skeleton_container.GetSkeleton();

    skeleton_container.OfferService(std::string{kTestNamePrefix});
    SendSamples(first_offer_samples, skeleton);
    WaitForNotificationOrFail(first_batch_of_samples_received_sync, stop_token, "first_batch_of_samples_received");

    skeleton.StopOfferService();
    stop_offer_done_sync->Notify();
    WaitForNotificationOrFail(ready_for_reoffer_sync, stop_token, "ready_for_reoffer");

    skeleton_container.OfferService(std::string{kTestNamePrefix});
    reoffer_done_sync->Notify();

    SendSamples(second_offer_samples, skeleton);
    WaitForNotificationOrFail(second_batch_of_samples_received_sync, stop_token, "second_batch_of_samples_received");
}

}  // namespace

void run_provider(const Scenario scenario, const score::cpp::stop_token& stop_token)
{
    switch (scenario)
    {
        case Scenario::kNoConnectedProxy:
            run_provider_no_connected_proxy(stop_token);
            break;
        case Scenario::kWithConnectedProxy:
            run_provider_with_connected_proxy(stop_token);
            break;
        case Scenario::kProxyDisconnectedBeforeReoffer:
            run_provider_proxy_disconnected_before_reoffer(stop_token);
            break;
        default:
            FailTest("skeleton_reoffer provider: Unknown scenario: ", static_cast<std::uint32_t>(scenario));
            break;
    }
}

}  // namespace score::mw::com::test::skeleton_reoffer
