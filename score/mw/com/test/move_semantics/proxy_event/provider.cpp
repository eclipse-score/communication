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
#include "score/mw/com/test/move_semantics/proxy_event/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/send_incrementing_sequence_of_samples.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/move_semantics/proxy_event/test_event_datatype.h"

#include <cstdint>
#include <string>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/proxy_event_move_semantics_interprocess_notification"};
const std::string kProviderWithdrawNotificationShmPath{"/proxy_event_move_semantics_provider_withdraw_notification"};

}  // namespace

void RunProvider(const std::size_t num_samples_to_send,
                 const std::size_t num_send_iterations,
                 const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::CreateUniquePtr(kInterprocessNotificationShmPath);
    auto provider_withdraw_synchronizer_result =
        ProcessSynchronizer::CreateUniquePtr(kProviderWithdrawNotificationShmPath);
    process_synchronizer_result->Reset();
    provider_withdraw_synchronizer_result->Reset();

    // Step 1. Create and offer the skeleton the consumer receives samples from. A second instance is also offered
    // so that the move-assign consumer scenarios can discover and create their second proxy before the move; the
    // move-construct scenarios simply ignore it. This keeps the provider generic and independent of the scenario.
    std::cout << "\nProvider: Step 1 - Create and offer skeletons" << std::endl;
    SkeletonContainer<ProxyMoveSemanticsSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    skeleton_container.OfferService("proxy_event_move_semantics");

    SkeletonContainer<ProxyMoveSemanticsSkeleton> moved_from_skeleton_container{};
    moved_from_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "proxy_event_move_semantics");
    moved_from_skeleton_container.OfferService("proxy_event_move_semantics");

    // Step 2. Wait for the consumer to complete its subscription and handler registration before sending any
    // samples. Without this synchronisation, the provider might send the entire first batch before the consumer
    // has called Subscribe(), after which LoLa SHM does not retroactively deliver pre-subscription samples to
    // the new subscriber, causing GetNewSamples to return 0 forever (hanging the test).
    std::cout << "\nProvider: Waiting for consumer to subscribe before sending first batch" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("proxy_event_move_semantics provider failed: waiting for consumer to subscribe was aborted");
    }
    process_synchronizer_result->Reset();

    // Step 3. Send one batch of samples per iteration and wait for the consumer to acknowledge each one. Before the
    // final batch the offer is withdrawn and re-offered so the consumer can unsubscribe and re-subscribe on a
    // withdrawn service (the consumer notifies once it has re-subscribed).
    for (std::size_t iteration = 0U; iteration < num_send_iterations; ++iteration)
    {
        const bool is_final_iteration = (iteration + 1U == num_send_iterations);
        if (is_final_iteration)
        {
            // Stop offering and immediately re-offer before notifying the consumer. This ensures the consumer
            // always subscribes to an already-offered service and never enters kSubscriptionPending. Entering
            // kSubscriptionPending and relying on LoLa's auto-reconnect is unreliable under concurrent load: the
            // state-change callback sometimes never fires, leaving WaitForSamples blocked until the stop_token
            // expires.
            std::cout << "\nProvider: Stop and re-offer skeleton so the consumer can unsubscribe and re-subscribe"
                      << std::endl;
            skeleton_container.GetSkeleton().StopOfferService();
            skeleton_container.OfferService("proxy_event_move_semantics");
            provider_withdraw_synchronizer_result->Notify();

            if (!process_synchronizer_result->WaitWithAbort(stop_token))
            {
                FailTest("proxy_event_move_semantics provider failed: waiting for consumer re-subscribe was aborted");
            }
            process_synchronizer_result->Reset();
        }

        const auto initial_value = static_cast<std::uint32_t>(iteration * num_samples_to_send) + 1U;
        std::cout << "\nProvider: Iteration " << (iteration + 1U) << " of " << num_send_iterations << " - Send "
                  << num_samples_to_send << " samples" << std::endl;
        SendIncrementingSequenceOfSamples(skeleton_container.GetSkeleton(), num_samples_to_send, initial_value);

        if (!process_synchronizer_result->WaitWithAbort(stop_token))
        {
            FailTest("proxy_event_move_semantics provider failed: waiting for consumer done was aborted");
        }
        process_synchronizer_result->Reset();
    }
}

}  // namespace score::mw::com::test
