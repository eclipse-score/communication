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
#include "score/mw/com/test/move_semantics/skeleton_event/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/move_semantics/skeleton_event/test_event_datatype.h"

#include <cstdint>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/skeleton_event_move_semantics_interprocess_notification"};

void SendSamples(SkeletonMoveSemanticsSkeleton& skeleton,
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

void RunProviderMoveConstructNotOfferedSkeleton(const score::cpp::stop_token& stop_token,
                                                const std::size_t number_of_samples_to_send_per_offer,
                                                ProcessSynchronizer& proxy_done_synchronizer)
{
    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<SkeletonMoveSemanticsSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_event_move_semantics");
    auto original_skeleton = skeleton_container.Extract();

    // Step 2. Move construct skeleton
    std::cout << "\nProvider: Step 2 - Move construct skeleton" << std::endl;
    auto moved_to_skeleton = std::move(original_skeleton);

    // Step 3. Offer skeleton
    std::cout << "\nProvider: Step 3 - Offer skeleton" << std::endl;
    auto offer_service_result = moved_to_skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_service_result.error());
    }

    // Step 4. Send n values
    std::cout << "\nProvider: Step 4 - Send " << number_of_samples_to_send_per_offer << " samples" << std::endl;
    SendSamples(moved_to_skeleton, number_of_samples_to_send_per_offer, 1U);

    // Step 5. Wait for consumer to notify that it received the first n values
    if (!proxy_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
    proxy_done_synchronizer.Reset();

    // Step 6. Stop offering skeleton
    std::cout << "\nProvider: Step 6 - Stop offering skeleton" << std::endl;
    moved_to_skeleton.StopOfferService();

    // Step 7. Offer skeleton again
    std::cout << "\nProvider: Step 7 - Offer skeleton again" << std::endl;
    offer_service_result = moved_to_skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_service_result.error());
    }

    // Step 8. Send n values
    std::cout << "\nProvider: Step 8 - Send " << number_of_samples_to_send_per_offer
              << " samples again after re-offering" << std::endl;
    SendSamples(moved_to_skeleton, number_of_samples_to_send_per_offer, number_of_samples_to_send_per_offer + 1U);

    // Step 9. Wait for consumer to notify that it received the second n values
    if (!proxy_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
}

void RunProviderMoveConstructOfferedSkeleton(const score::cpp::stop_token& stop_token,
                                             const std::size_t number_of_samples_to_send_per_offer,
                                             ProcessSynchronizer& proxy_done_synchronizer)
{
    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<SkeletonMoveSemanticsSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_event_move_semantics");

    // Step 2. Offer skeleton
    std::cout << "\nProvider: Step 2 - Offer skeleton" << std::endl;
    skeleton_container.OfferService("skeleton_event_move_semantics");

    // Step 3. Send n values
    std::cout << "\nProvider: Step 3 - Send " << number_of_samples_to_send_per_offer << " samples" << std::endl;
    SendSamples(skeleton_container.GetSkeleton(), number_of_samples_to_send_per_offer, 1U);

    // Step 4. Wait for consumer to notify that it received the first n values
    if (!proxy_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
    proxy_done_synchronizer.Reset();

    // Step 5. Move construct skeleton while offered
    std::cout << "\nProvider: Step 5 - Move construct skeleton while offered" << std::endl;
    auto moved_to_skeleton = std::move(skeleton_container.GetSkeleton());

    // Step 6. Send n values
    std::cout << "\nProvider: Step 6 - Send " << number_of_samples_to_send_per_offer
              << " samples again after re-offering" << std::endl;
    SendSamples(moved_to_skeleton, number_of_samples_to_send_per_offer, number_of_samples_to_send_per_offer + 1U);

    // Step 7. Wait for consumer to notify that it received the second n values
    if (!proxy_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
    proxy_done_synchronizer.Reset();

    // Step 8. Stop offering skeleton
    std::cout << "\nProvider: Step 8 - Stop offering skeleton" << std::endl;
    moved_to_skeleton.StopOfferService();

    // Step 9. Offer skeleton again
    std::cout << "\nProvider: Step 9 - Offer skeleton again" << std::endl;
    auto offer_service_result = moved_to_skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_service_result.error());
    }

    // Step 10. Send n values
    std::cout << "\nProvider: Step 10 - Send " << number_of_samples_to_send_per_offer
              << " samples again after re-offering" << std::endl;
    SendSamples(moved_to_skeleton, number_of_samples_to_send_per_offer, number_of_samples_to_send_per_offer * 2U + 1U);

    // Step 11. Wait for consumer to notify that it received the third n values
    if (!proxy_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
    proxy_done_synchronizer.Reset();
}

void RunProviderMoveAssignNotOfferedSkeleton(const score::cpp::stop_token& stop_token,
                                             const std::size_t number_of_samples_to_send_per_offer,
                                             ProcessSynchronizer& proxy_done_synchronizer)
{
    // Step 1. Create two skeletons
    std::cout << "\nProvider: Step 1 - Create two skeletons" << std::endl;
    SkeletonContainer<SkeletonMoveSemanticsSkeleton> moved_from_skeleton_container{};
    moved_from_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_event_move_semantics");
    auto moved_from_skeleton = moved_from_skeleton_container.Extract();

    SkeletonContainer<SkeletonMoveSemanticsSkeleton> moved_to_skeleton_container{};
    moved_to_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "skeleton_event_move_semantics");
    auto moved_to_skeleton = moved_to_skeleton_container.Extract();

    // Step 2. Move assign skeleton
    std::cout << "\nProvider: Step 2 - Move assign skeleton" << std::endl;
    moved_to_skeleton = std::move(moved_from_skeleton);

    // Step 3. Offer skeleton
    std::cout << "\nProvider: Step 3 - Offer skeleton" << std::endl;
    auto offer_service_result = moved_to_skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_service_result.error());
    }

    // Step 4. Send n values
    std::cout << "\nProvider: Step 4 - Send " << number_of_samples_to_send_per_offer << " samples" << std::endl;
    SendSamples(moved_to_skeleton, number_of_samples_to_send_per_offer, 1U);

    // Step 5. Wait for consumer to notify that it received the first n values
    if (!proxy_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
    proxy_done_synchronizer.Reset();

    // Step 6. Stop offering skeleton
    std::cout << "\nProvider: Step 6 - Stop offering skeleton" << std::endl;
    moved_to_skeleton.StopOfferService();

    // Step 7. Offer skeleton again
    std::cout << "\nProvider: Step 7 - Offer skeleton again" << std::endl;
    offer_service_result = moved_to_skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_service_result.error());
    }

    // Step 8. Send n values
    std::cout << "\nProvider: Step 8 - Send " << number_of_samples_to_send_per_offer
              << " samples again after re-offering" << std::endl;
    SendSamples(moved_to_skeleton, number_of_samples_to_send_per_offer, number_of_samples_to_send_per_offer + 1U);

    // Step 9. Wait for consumer to notify that it received the second n values
    if (!proxy_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for consumer done was aborted");
    }
}

void RunProviderMoveAssignOfferedSkeleton(const score::cpp::stop_token& stop_token,
                                          const std::size_t number_of_samples_to_send_per_offer,
                                          ProcessSynchronizer& moved_to_proxy_done_synchronizer_result,
                                          ProcessSynchronizer& moved_from_proxy_done_synchronizer_result)
{
    // Step 1. Create two skeletons
    std::cout << "\nProvider: Step 1 - Create two skeletons" << std::endl;
    SkeletonContainer<SkeletonMoveSemanticsSkeleton> moved_from_skeleton_container{};
    moved_from_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_event_move_semantics");

    SkeletonContainer<SkeletonMoveSemanticsSkeleton> moved_to_skeleton_container{};
    moved_to_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "skeleton_event_move_semantics");

    // Step 2. Offer both skeletons
    std::cout << "\nProvider: Step 2 - Offer both skeletons" << std::endl;
    moved_from_skeleton_container.OfferService("skeleton_event_move_semantics");
    moved_to_skeleton_container.OfferService("skeleton_event_move_semantics");

    // Step 3. Send n values from both skeletons
    std::cout << "\nProvider: Step 3 - Send " << number_of_samples_to_send_per_offer << " samples from both skeletons"
              << std::endl;
    SendSamples(moved_to_skeleton_container.GetSkeleton(), number_of_samples_to_send_per_offer, 1U);
    SendSamples(moved_from_skeleton_container.GetSkeleton(), number_of_samples_to_send_per_offer, 1U);

    // Step 4. Wait for both consumers to notify that they received the first n values
    if (!moved_to_proxy_done_synchronizer_result.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for moved-to consumer done was aborted");
    }
    moved_to_proxy_done_synchronizer_result.Reset();
    if (!moved_from_proxy_done_synchronizer_result.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for moved-from consumer done was aborted");
    }
    moved_from_proxy_done_synchronizer_result.Reset();

    // Step 5. Move assign skeleton
    std::cout << "\nProvider: Step 5 - Move assign skeleton" << std::endl;
    auto moved_from_skeleton = moved_from_skeleton_container.Extract();
    auto moved_to_skeleton = moved_to_skeleton_container.Extract();
    moved_to_skeleton = std::move(moved_from_skeleton);

    // Step 6. Send n values from moved-to skeleton
    std::cout << "\nProvider: Step 6 - Send " << number_of_samples_to_send_per_offer
              << " samples from moved-to skeleton" << std::endl;
    SendSamples(moved_to_skeleton, number_of_samples_to_send_per_offer, number_of_samples_to_send_per_offer + 1U);

    // Step 7. Stop offering moved-to skeleton
    std::cout << "\nProvider: Step 7 - Stop offering moved-to skeleton" << std::endl;
    moved_to_skeleton.StopOfferService();

    // Step 8. Offer moved-to skeleton again
    std::cout << "\nProvider: Step 8 - Offer moved-to skeleton again" << std::endl;
    const auto offer_service_result = moved_to_skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_service_result.error());
    }

    // Step 9. Send n values
    std::cout << "\nProvider: Step 9 - Send " << number_of_samples_to_send_per_offer
              << " samples again after re-offering" << std::endl;
    SendSamples(moved_to_skeleton, number_of_samples_to_send_per_offer, 2 * number_of_samples_to_send_per_offer + 1U);

    // Step 10. Wait for moved-to consumer to notify that it received the third n values
    if (!moved_to_proxy_done_synchronizer_result.WaitWithAbort(stop_token))
    {
        FailTest("skeleton_event_move_semantics provider failed: waiting for moved-to consumer done was aborted");
    }
}

}  // namespace

void RunProvider(const SkeletonMoveScenario& scenario,
                 const std::size_t num_samples_to_send,
                 const score::cpp::stop_token& stop_token)
{
    const auto moved_to_name = filesystem::Path{kInstanceSpecifierMovedTo.ToString()}.Filename().Native();
    auto moved_to_proxy_done_synchronizer_result =
        ProcessSynchronizer::Create(kInterprocessNotificationShmPath + std::string{moved_to_name});
    if (!moved_to_proxy_done_synchronizer_result.has_value())
    {
        FailTest("skeleton_event_move_semantics provider failed: could not create ready synchronizer");
    }

    switch (scenario)
    {
        case SkeletonMoveScenario::kMoveConstructNotOffered:
        {
            RunProviderMoveConstructNotOfferedSkeleton(
                stop_token, num_samples_to_send, moved_to_proxy_done_synchronizer_result.value());
            break;
        }
        case SkeletonMoveScenario::kMoveConstructOffered:
        {
            RunProviderMoveConstructOfferedSkeleton(
                stop_token, num_samples_to_send, moved_to_proxy_done_synchronizer_result.value());
            break;
        }
        case SkeletonMoveScenario::kMoveAssignNotOffered:
        {
            RunProviderMoveAssignNotOfferedSkeleton(
                stop_token, num_samples_to_send, moved_to_proxy_done_synchronizer_result.value());
            break;
        }
        case SkeletonMoveScenario::kMoveAssignOffered:
        {
            const auto moved_from_name = filesystem::Path{kInstanceSpecifierMovedFrom.ToString()}.Filename().Native();
            auto moved_from_proxy_done_synchronizer_result =
                ProcessSynchronizer::Create(kInterprocessNotificationShmPath + std::string{moved_from_name});
            if (!moved_from_proxy_done_synchronizer_result.has_value())
            {
                FailTest("skeleton_event_move_semantics provider failed: could not create ready synchronizer");
            }
            RunProviderMoveAssignOfferedSkeleton(stop_token,
                                                 num_samples_to_send,
                                                 moved_to_proxy_done_synchronizer_result.value(),
                                                 moved_from_proxy_done_synchronizer_result.value());
            break;
        }
        case SkeletonMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
            FailTest("skeleton_event_move_semantics provider failed: unknown scenario");
    }
}

}  // namespace score::mw::com::test
