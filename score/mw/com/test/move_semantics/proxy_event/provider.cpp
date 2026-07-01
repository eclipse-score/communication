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
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/move_semantics/proxy_event/test_event_datatype.h"

#include <cstdint>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/proxy_event_move_semantics_interprocess_notification"};

void SendSamples(ProxyMoveSemanticsSkeleton& skeleton,
                 const std::size_t number_of_samples_to_send_per_offer,
                 const std::uint32_t initial_value)
{
    std::cout << "\nProvider: Sending " << number_of_samples_to_send_per_offer << " samples" << std::endl;
    for (std::uint32_t i = 0; i < number_of_samples_to_send_per_offer; ++i)
    {
        auto send_result = skeleton.moved_event_.Send(i + initial_value);
        if (!send_result.has_value())
        {
            FailTest("Provider: Send failed: ", send_result.error());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void RunProviderMoveConstructProxyBeforeSubscribe(const score::cpp::stop_token& stop_token,
                                                  const std::size_t number_of_samples_to_send_per_offer,
                                                  ProcessSynchronizer& proxy_done_synchronizer)
{
    // Step 1. Create skeleton and offer service
    std::cout << "\nProvider: Step 1 - Create and offer skeleton" << std::endl;
    SkeletonContainer<ProxyMoveSemanticsSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    skeleton_container.OfferService("proxy_event_move_semantics");

    // Step 2. Send first n values
    std::cout << "\nProvider: Step 2 - Send " << number_of_samples_to_send_per_offer << " samples" << std::endl;
    SendSamples(skeleton_container.GetSkeleton(), number_of_samples_to_send_per_offer, 1U);

    // Step 3. Wait for consumer to notify that it received the first n values
    if (!proxy_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("proxy_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
    proxy_done_synchronizer.Reset();

    // Step 4. Stop and re-offer to exercise subscription continuity on moved proxy
    std::cout << "\nProvider: Step 4 - Stop and re-offer skeleton" << std::endl;
    skeleton_container.GetSkeleton().StopOfferService();
    skeleton_container.OfferService("proxy_event_move_semantics");

    // Step 5. Send second n values
    std::cout << "\nProvider: Step 5 - Send " << number_of_samples_to_send_per_offer
              << " samples again after re-offering" << std::endl;
    SendSamples(skeleton_container.GetSkeleton(),
                number_of_samples_to_send_per_offer,
                number_of_samples_to_send_per_offer + 1U);

    // Step 6. Wait for consumer to notify that it received the second n values
    if (!proxy_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("proxy_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
}

}  // namespace

void RunProvider(const ProxyMoveScenario& scenario,
                 const std::size_t num_samples_to_send,
                 const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result =
        ProcessSynchronizer::CreateUniquePtr(kInterprocessNotificationShmPath + std::string{"MoveEventInterface"});

    if (scenario == ProxyMoveScenario::kMoveConstructBeforeSubscribe)
    {
        RunProviderMoveConstructProxyBeforeSubscribe(stop_token, num_samples_to_send, *process_synchronizer_result);
        return;
    }

    FailTest("Unknown proxy move scenario in provider");
}

}  // namespace score::mw::com::test
