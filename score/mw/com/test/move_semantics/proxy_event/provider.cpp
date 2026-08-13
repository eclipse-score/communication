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
#include "score/mw/com/test/move_semantics/proxy_event/proxy_event_move_semantics_interface.h"

#include <cstdint>
#include <string>

namespace score::mw::com::test
{
namespace
{

const std::string kConsumerBatchReceivedNotificationShmPath{
    "/proxy_event_move_semantics_consumer_batch_received_notification"};
const std::string kProviderWithdrawNotificationShmPath{"/proxy_event_move_semantics_provider_withdraw_notification"};
const std::string kConsumerResubscribeNotificationShmPath{
    "/proxy_event_move_semantics_consumer_resubscribe_notification"};

}  // namespace

void RunProvider(const std::size_t num_samples_to_send,
                 const std::size_t num_send_iterations,
                 const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerBatchReceivedNotificationShmPath);
    auto provider_withdraw_synchronizer_result = ProcessSynchronizer::Create(kProviderWithdrawNotificationShmPath);
    auto resubscribe_synchronizer_result = ProcessSynchronizer::Create(kConsumerResubscribeNotificationShmPath);
    process_synchronizer_result->Reset();
    provider_withdraw_synchronizer_result->Reset();
    resubscribe_synchronizer_result->Reset();

    // Step 1. Create and offer both skeletons. The second instance is only used by the move-assign scenarios.
    std::cout << "\nProvider: Step 1 - Create and offer skeletons" << std::endl;
    SkeletonContainer<ProxyMoveSemanticsSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    skeleton_container.OfferService("proxy_event_move_semantics");

    SkeletonContainer<ProxyMoveSemanticsSkeleton> moved_from_skeleton_container{};
    moved_from_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "proxy_event_move_semantics");
    moved_from_skeleton_container.OfferService("proxy_event_move_semantics");

    // Step 2. Send num_send_iterations - 1 batches, waiting for the consumer to acknowledge each one.
    for (std::size_t iteration = 0U; iteration + 1U < num_send_iterations; ++iteration)
    {
        const auto initial_value = static_cast<std::uint32_t>(iteration * num_samples_to_send) + 1U;
        std::cout << "\nProvider: Iteration " << (iteration + 1U) << " of " << num_send_iterations << " - Send "
                  << num_samples_to_send << " samples" << std::endl;
        SendIncrementingSequenceOfSamples(
            skeleton_container.GetSkeleton().moved_event_, num_samples_to_send, initial_value);

        if (!process_synchronizer_result->WaitWithAbort(stop_token))
        {
            FailTest("proxy_event_move_semantics provider failed: waiting for consumer done was aborted");
        }
        process_synchronizer_result->Reset();
    }

    // Step 3. Withdraw and re-offer the service so the consumer can unsubscribe and re-subscribe, then wait for
    // the consumer to notify that it has re-subscribed.
    std::cout << "\nProvider: Stop and re-offer skeleton so the consumer can unsubscribe and re-subscribe" << std::endl;
    skeleton_container.GetSkeleton().StopOfferService();
    skeleton_container.OfferService("proxy_event_move_semantics");
    provider_withdraw_synchronizer_result->Notify();

    if (!resubscribe_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("proxy_event_move_semantics provider failed: waiting for consumer re-subscribe was aborted");
    }
    resubscribe_synchronizer_result->Reset();

    // Step 4. Send the final batch of samples and wait for the consumer to acknowledge it.
    const auto final_iteration = num_send_iterations - 1U;
    const auto initial_value = static_cast<std::uint32_t>(final_iteration * num_samples_to_send) + 1U;
    std::cout << "\nProvider: Iteration " << num_send_iterations << " of " << num_send_iterations << " - Send "
              << num_samples_to_send << " samples" << std::endl;
    SendIncrementingSequenceOfSamples(
        skeleton_container.GetSkeleton().moved_event_, num_samples_to_send, initial_value);

    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("proxy_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
    process_synchronizer_result->Reset();
}

}  // namespace score::mw::com::test
