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
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/move_semantics/proxy_event/proxy_event_move_semantics_interface.h"
#include "score/mw/com/test/move_semantics/proxy_event/test_parameters.h"

#include <cstdint>
#include <string>

namespace score::mw::com::test
{
namespace
{

const std::string kConsumerBatchReceivedNotificationShmPath{
    "/proxy_event_move_semantics_consumer_batch_received_notification"};
const std::string kReofferTriggerNotificationShmPath{"/proxy_event_move_semantics_provider_withdraw_notification"};

}  // namespace

void RunProvider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerBatchReceivedNotificationShmPath);
    auto reoffer_trigger_synchronizer = ProcessSynchronizer::Create(kReofferTriggerNotificationShmPath);

    // Step 1. Create and offer both skeletons. The second instance is only used by the move-assign scenarios.
    std::cout << "\nProvider: Step 1 - Create and offer skeletons" << std::endl;
    SkeletonContainer<ProxyMoveSemanticsSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    skeleton_container.OfferService("proxy_event_move_semantics");

    SkeletonContainer<ProxyMoveSemanticsSkeleton> moved_from_skeleton_container{};
    moved_from_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "proxy_event_move_semantics");
    moved_from_skeleton_container.OfferService("proxy_event_move_semantics");

    // Step 2. Send First batch of samples, waiting for the consumer to acknowledge each one.
    std::cout << "\nProvider: Step 2 - Sending the first batch of samples" << std::endl;
    for (const auto sample : kInitialSamplesToSend)
    {
        skeleton_container.GetSkeleton().moved_event_.Send(sample);
    }
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(
            "proxy_event_move_semantics provider failed: waiting for consumer to notify that it received the first "
            "batch of samples was aborted");
    }

    // Step 3. Stop offering the service and re-offer.
    std::cout << "\nProvider: Step 3 - Stop and re-offer the Service" << std::endl;
    skeleton_container.GetSkeleton().StopOfferService();
    skeleton_container.OfferService("proxy_event_move_semantics");

    // Step 4. Sending the second batch of samples.
    std::cout << "\nProvider: Step 4 - Sending the second batch of samples" << std::endl;
    for (const auto sample : kSamplesToSendAfterReOffer)
    {
        skeleton_container.GetSkeleton().moved_event_.Send(sample);
        std::cout << "Provider: sent sample value = " << sample << std::endl;
    }

    //  Step 5. Wait for the consumer to tell that it has received the second batch of samples.
    std::cout << "\nProvider: Step 5 - Wait for the consumer to tell that it has received the second batch of samples"
              << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(
            "proxy_event_move_semantics provider failed: waiting for consumer to receive the second batch of samples "
            "was aborted");
    }
}
}  // namespace score::mw::com::test
