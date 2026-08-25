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
#include "score/mw/com/test/move_semantics/skeleton_field/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/move_semantics/skeleton_field/test_field_datatype.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/skeleton_field_move_semantics_interprocess_notification"};
const std::string kAboutToCallShmPath{"/skeleton_field_move_semantics_about_to_call"};

void ValueTransformSetHandler(std::int32_t& value) noexcept
{
    value = (value * 2) + 1;
}

void RegisterSetHandler(SkeletonFieldMoveSemanticsSkeleton& skeleton)
{
    const auto register_handler_result =
        skeleton.moved_field_.RegisterSetHandler([](std::int32_t& value) noexcept { ValueTransformSetHandler(value); });
    if (!register_handler_result.has_value())
    {
        FailTest("Provider: Unable to register set handler: ", register_handler_result.error());
    }
}

void UpdateInitialValue(SkeletonFieldMoveSemanticsSkeleton& skeleton)
{
    const auto update_result = skeleton.moved_field_.Update(kInitialValue);
    if (!update_result.has_value())
    {
        FailTest("Provider: Unable to update field with initial value: ", update_result.error());
    }
}

void SendValues(SkeletonFieldMoveSemanticsSkeleton& skeleton)
{
    for (const auto value_to_send : kValuesToSend)
    {
        const auto update_result = skeleton.moved_field_.Update(value_to_send);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }
}

/// \brief Sleeps a random duration in [0, window] before returning, so that the caller's subsequent action (the
/// move) lands at an unpredictable point during the consumer's live API call sequence.
void SleepRandomRaceDelay(const std::chrono::microseconds& window)
{
    static thread_local std::mt19937 random_engine{std::random_device{}()};
    std::uniform_int_distribution<std::int64_t> distribution{0, window.count()};
    const auto delay = std::chrono::microseconds{distribution(random_engine)};
    std::this_thread::sleep_for(delay);
}

void RunMoveConstructBeforeOfferProvider(const score::cpp::stop_token& stop_token)
{
    auto done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create done ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<SkeletonFieldMoveSemanticsSkeleton> moved_to_skeleton_container{};
    moved_to_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_field_move_semantics");

    RegisterSetHandler(moved_to_skeleton_container.GetSkeleton());
    UpdateInitialValue(moved_to_skeleton_container.GetSkeleton());

    // Step 2. Move construct skeleton before offering
    std::cout << "\nProvider: Step 2 - Move construct skeleton before offer" << std::endl;
    auto moved_skeleton = std::move(moved_to_skeleton_container.GetSkeleton());
    
    // Step 3. Offer skeleton
    std::cout << "\nProvider: Step 3 - Offer skeleton" << std::endl;
    const auto offer_service_result = moved_skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_service_result.error());
    }

    // Step 4. Send values
    std::cout << "\nProvider: Step 4 - Send updated values" << std::endl;
    SendValues(moved_skeleton);

    // Step 5. Wait for consumer to notify it is done with the verification sequence
    std::cout << "\nProvider: Step 5 - Wait for consumer done notification" << std::endl;
    if (!done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (done) was stopped by stop_token instead of notification");
    }

    // Step 6. Stop offering skeleton
    std::cout << "\nProvider: Step 6 - Stop offering skeleton" << std::endl;
    moved_skeleton.StopOfferService();
}

void RunMoveAssignBeforeOfferProvider(const score::cpp::stop_token& stop_token)
{
    auto done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create done ProcessSynchronizer");
    }

    // Step 1. Create two skeletons
    std::cout << "\nProvider: Step 1 - Create two skeletons" << std::endl;
    SkeletonContainer<SkeletonFieldMoveSemanticsSkeleton> moved_to_skeleton_container{};
    moved_to_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_field_move_semantics");

    RegisterSetHandler(moved_to_skeleton_container.GetSkeleton());
    UpdateInitialValue(moved_to_skeleton_container.GetSkeleton());

    SkeletonContainer<SkeletonFieldMoveSemanticsSkeleton> moved_from_skeleton_container{};
    moved_from_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "skeleton_field_move_semantics");
    RegisterSetHandler(moved_from_skeleton_container.GetSkeleton());
    UpdateInitialValue(moved_from_skeleton_container.GetSkeleton());
    auto moved_skeleton = moved_from_skeleton_container.Extract();

    // Step 2. Move assign skeleton before offering
    std::cout << "\nProvider: Step 2 - Move assign skeleton before offer" << std::endl;
    moved_skeleton = std::move(moved_to_skeleton_container.GetSkeleton());

    // Step 3. Offer skeleton
    std::cout << "\nProvider: Step 3 - Offer skeleton" << std::endl;
    const auto offer_service_result = moved_skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_service_result.error());
    }

    // Step 4. Send values
    std::cout << "\nProvider: Step 4 - Send updated values" << std::endl;
    SendValues(moved_skeleton);

    // Step 5. Wait for consumer to notify it is done with the verification sequence
    std::cout << "\nProvider: Step 5 - Wait for consumer done notification" << std::endl;
    if (!done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (done) was stopped by stop_token instead of notification");
    }

    // Step 6. Stop offering skeleton
    std::cout << "\nProvider: Step 6 - Stop offering skeleton" << std::endl;
    moved_skeleton.StopOfferService();
}

void RunMoveConstructAfterOfferProvider(const score::cpp::stop_token& stop_token)
{
    auto done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create done ProcessSynchronizer");
    }
    auto about_to_call_synchronizer_result = ProcessSynchronizer::Create(kAboutToCallShmPath);
    if (!about_to_call_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create about-to-call ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<SkeletonFieldMoveSemanticsSkeleton> moved_to_skeleton_container{};
    moved_to_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_field_move_semantics");

    RegisterSetHandler(moved_to_skeleton_container.GetSkeleton());
    UpdateInitialValue(moved_to_skeleton_container.GetSkeleton());

    // Step 2. Offer skeleton (no move yet)
    std::cout << "\nProvider: Step 2 - Offer skeleton" << std::endl;
    moved_to_skeleton_container.OfferService("skeleton_field_move_semantics");

    // Step 3. Send values
    std::cout << "\nProvider: Step 3 - Send updated values" << std::endl;
    SendValues(moved_to_skeleton_container.GetSkeleton());

    // Step 4. Wait for consumer to signal it is about to start the WaitForSamples->Set->Get sequence
    std::cout << "\nProvider: Step 4 - Wait for consumer about-to-call notification" << std::endl;
    if (!about_to_call_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (about-to-call) was stopped by stop_token instead of notification");
    }

    // Step 5. Sleep a single random delay, then move construct at random point during the consumer's 
    // live WaitForSamples->Set->Get sequence
    std::cout << "\nProvider: Step 5 - Sleep random race delay, then move construct skeleton while offered"
              << std::endl;
    SleepRandomRaceDelay(kSequenceRaceWindowUs);
    auto moved_skeleton = std::move(moved_to_skeleton_container.GetSkeleton());

    // Step 6. Wait for consumer to notify it is done with the verification sequence
    std::cout << "\nProvider: Step 6 - Wait for consumer done notification" << std::endl;
    if (!done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (done) was stopped by stop_token instead of notification");
    }

    // Step 7. Stop offering skeleton
    std::cout << "\nProvider: Step 7 - Stop offering skeleton" << std::endl;
    moved_skeleton.StopOfferService();
}

void RunMoveAssignAfterOfferProvider(const score::cpp::stop_token& stop_token)
{
    auto done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create done ProcessSynchronizer");
    }
    auto about_to_call_synchronizer_result = ProcessSynchronizer::Create(kAboutToCallShmPath);
    if (!about_to_call_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create about-to-call ProcessSynchronizer");
    }

    // Step 1. Create two skeletons
    std::cout << "\nProvider: Step 1 - Create two skeletons" << std::endl;
    SkeletonContainer<SkeletonFieldMoveSemanticsSkeleton> moved_to_skeleton_container{};
    moved_to_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_field_move_semantics");

    RegisterSetHandler(moved_to_skeleton_container.GetSkeleton());
    UpdateInitialValue(moved_to_skeleton_container.GetSkeleton());

    SkeletonContainer<SkeletonFieldMoveSemanticsSkeleton> moved_from_skeleton_container{};
    moved_from_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "skeleton_field_move_semantics");
    RegisterSetHandler(moved_from_skeleton_container.GetSkeleton());
    UpdateInitialValue(moved_from_skeleton_container.GetSkeleton());

    // Step 2. Offer both skeletons
    std::cout << "\nProvider: Step 2 - Offer skeleton" << std::endl;
    moved_to_skeleton_container.OfferService("skeleton_field_move_semantics");
    moved_from_skeleton_container.OfferService("skeleton_field_move_semantics");

    auto moved_skeleton = moved_from_skeleton_container.Extract();

    // Step 3. Send values
    std::cout << "\nProvider: Step 3 - Send updated values" << std::endl;
    SendValues(moved_to_skeleton_container.GetSkeleton());

    // Step 4. Wait for consumer to signal it is about to start the WaitForSamples->Set->Get sequence
    std::cout << "\nProvider: Step 4 - Wait for consumer about-to-call notification" << std::endl;
    if (!about_to_call_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (about-to-call) was stopped by stop_token instead of notification");
    }

    // Step 5. Sleep a single random delay, then move assign at random point during the consumer's 
    // live WaitForSamples->Set->Get sequence.
    std::cout << "\nProvider: Step 5 - Sleep random race delay, then move assign skeleton while offered" << std::endl;
    SleepRandomRaceDelay(kSequenceRaceWindowUs);
    moved_skeleton = std::move(moved_to_skeleton_container.GetSkeleton());

    // Step 6. Wait for consumer to notify it is done with the verification sequence
    std::cout << "\nProvider: Step 6 - Wait for consumer done notification" << std::endl;
    if (!done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (done) was stopped by stop_token instead of notification");
    }

    // Step 7. Stop offering skeleton
    std::cout << "\nProvider: Step 7 - Stop offering skeleton" << std::endl;
    moved_skeleton.StopOfferService();
}

}  // namespace

void RunProvider(const SkeletonFieldMoveScenario& scenario, const score::cpp::stop_token& stop_token)
{
    switch (scenario)
    {
        case SkeletonFieldMoveScenario::kMoveConstructBeforeOffer:
            RunMoveConstructBeforeOfferProvider(stop_token);
            break;
        case SkeletonFieldMoveScenario::kMoveConstructAfterOffer:
            RunMoveConstructAfterOfferProvider(stop_token);
            break;
        case SkeletonFieldMoveScenario::kMoveAssignBeforeOffer:
            RunMoveAssignBeforeOfferProvider(stop_token);
            break;
        case SkeletonFieldMoveScenario::kMoveAssignAfterOffer:
            RunMoveAssignAfterOfferProvider(stop_token);
            break;
        case SkeletonFieldMoveScenario::kNumberOfScenarios:
        default:
            FailTest("Provider: Unknown scenario");
            break;
    }
}

}  // namespace score::mw::com::test
