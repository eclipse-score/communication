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
#include "score/mw/com/test/move_semantics/skeleton_method/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/move_semantics/skeleton_method/test_method_datatype.h"
#include "score/mw/com/test/move_semantics/skeleton_method/test_parameters.h"

#include "score/concurrency/notification.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <utility>

namespace score::mw::com::test
{
namespace
{

constexpr auto kNotificationTimeout = std::chrono::milliseconds{5000};

void SleepRandomDuration()
{
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<std::int64_t> dist{0, kMaxFuzzySleepDuration.count()};
    const auto delay_ms = std::chrono::milliseconds{dist(gen)};
    std::cout << "\nProvider: Sleeping " << delay_ms.count() << "ms before move" << std::endl;
    std::this_thread::sleep_for(delay_ms);
}

template <typename Method, typename Handler>
void RegisterHandlerOrFail(Method& method, Handler handler, const char* error_msg)
{
    const auto result = method.RegisterHandler(std::move(handler));
    if (!result.has_value())
    {
        FailTest(error_msg, result.error());
    }
}

template <typename Skeleton>
void OfferSkeletonOrFail(Skeleton& skeleton, const char* error_msg)
{
    const auto result = skeleton.OfferService();
    if (!result.has_value())
    {
        FailTest(error_msg, result.error());
    }
}

void WaitOrFail(concurrency::Notification& notification,
                const score::cpp::stop_token& stop_token,
                const char* error_msg)
{
    if (!notification.waitForWithAbort(kNotificationTimeout, stop_token))
    {
        FailTest(error_msg);
    }
}

void RunProviderMoveConstructBeforeOffered(const score::cpp::stop_token& stop_token)
{
    // Step 1. Create Skeleton A
    std::cout << "\nProvider: Step 1 - Create Skeleton A" << std::endl;
    SkeletonContainer<SkeletonMethodMoveSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");
    auto skeleton_a = skeleton_container.Extract();

    // Step 2. Move #1: Skeleton B = std::move(Skeleton A)  [before registering any handler]
    std::cout << "\nProvider: Step 2 - Move #1: Skeleton B = std::move(Skeleton A)" << std::endl;
    auto skeleton_b = std::move(skeleton_a);

    // Step 3. Register Handler A on Skeleton B. Its body notifies handler_a_called when called.
    std::cout << "\nProvider: Step 3 - Register Handler A (a + b) on Skeleton B" << std::endl;
    concurrency::Notification handler_a_called{};
    RegisterHandlerOrFail(
        skeleton_b.moved_method_,
        [&handler_a_called](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a + b;
            handler_a_called.notify();
        },
        "Provider: Failed to register Handler A: ");

    // Step 4. Move #2: Skeleton C = std::move(Skeleton B)  [after registering the handler]
    std::cout << "\nProvider: Step 4 - Move #2: Skeleton C = std::move(Skeleton B)" << std::endl;
    auto skeleton_c = std::move(skeleton_b);

    // Step 5. Offer Skeleton C
    std::cout << "\nProvider: Step 5 - Offer Skeleton C" << std::endl;
    OfferSkeletonOrFail(skeleton_c, "Provider: OfferService failed: ");

    // Step 6. Wait until Handler A has been called at least once.
    std::cout << "\nProvider: Step 6 - Wait for Handler A to be called" << std::endl;
    WaitOrFail(handler_a_called, stop_token, "Provider: Notification wait (Handler A) timed out");

    // Step 7. Register Handler B on Skeleton C. Its body notifies handler_b_called when called.
    std::cout << "\nProvider: Step 7 - Register Handler B (a * b) on Skeleton C" << std::endl;
    concurrency::Notification handler_b_called{};
    RegisterHandlerOrFail(
        skeleton_c.moved_method_,
        [&handler_b_called](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a * b;
            handler_b_called.notify();
        },
        "Provider: Failed to register Handler B: ");

    // Step 8. Wait until Handler B has been called at least once before finishing.
    std::cout << "\nProvider: Step 8 - Wait for Handler B to be called" << std::endl;
    WaitOrFail(handler_b_called, stop_token, "Provider: Notification wait (Handler B) timed out");

    // Step 9. Return: Skeleton C's destructor calls StopOfferService().
    std::cout << "\nProvider: Step 9 - Finishing" << std::endl;
}

void RunProviderMoveConstructAfterOffered(const score::cpp::stop_token& stop_token)
{
    // Step 1. Create Skeleton A
    std::cout << "\nProvider: Step 1 - Create Skeleton A" << std::endl;
    SkeletonContainer<SkeletonMethodMoveSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");
    auto skeleton_a = skeleton_container.Extract();

    // Step 2. Register Handler A on Skeleton A. Its body notifies handler_a_called when called.
    std::cout << "\nProvider: Step 2 - Register Handler A (a + b)" << std::endl;
    concurrency::Notification handler_a_called{};
    RegisterHandlerOrFail(
        skeleton_a.moved_method_,
        [&handler_a_called](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a + b;
            handler_a_called.notify();
        },
        "Provider: Failed to register Handler A: ");

    // Step 3. Offer Skeleton A
    std::cout << "\nProvider: Step 3 - Offer Skeleton A" << std::endl;
    OfferSkeletonOrFail(skeleton_a, "Provider: OfferService failed: ");

    // Step 4. Wait until Handler A has been called at least once before proceeding.
    std::cout << "\nProvider: Step 4 - Wait for Handler A to be called" << std::endl;
    WaitOrFail(handler_a_called, stop_token, "Provider: Notification wait (Handler A) timed out");

    // Step 5. Sleep a random duration (0-500ms). Fuzzy Scenario: Move operation is done at random point while
    //         the consumer is independently calling the method in a loop.
    SleepRandomDuration();

    // Step 6. Move construct: Skeleton B = std::move(Skeleton A)  [already offered, random timing]
    std::cout << "\nProvider: Step 6 - Move construct Skeleton B = std::move(Skeleton A)" << std::endl;
    auto skeleton_b = std::move(skeleton_a);

    // Step 7. Register Handler B on Skeleton B. Its body notifies handler_b_called when called.
    std::cout << "\nProvider: Step 7 - Register Handler B (a * b) on Skeleton B" << std::endl;
    concurrency::Notification handler_b_called{};
    RegisterHandlerOrFail(
        skeleton_b.moved_method_,
        [&handler_b_called](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a * b;
            handler_b_called.notify();
        },
        "Provider: Failed to register Handler B: ");

    // Step 8. Wait until Handler B has been called at least once before finishing.
    std::cout << "\nProvider: Step 8 - Wait for Handler B to be called" << std::endl;
    WaitOrFail(handler_b_called, stop_token, "Provider: Notification wait (Handler B) timed out");

    // Step 9. Return: Skeleton B's destructor calls StopOfferService().
    std::cout << "\nProvider: Step 9 - Finishing" << std::endl;
}

void RunProviderMoveAssignBeforeOffered(const score::cpp::stop_token& stop_token)
{
    // Step 1. Create Skeleton A  and Skeleton B.
    std::cout << "\nProvider: Step 1 - Create Skeleton A and placeholder Skeleton B" << std::endl;
    SkeletonContainer<SkeletonMethodMoveSkeleton> skeleton_a_container{};
    skeleton_a_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");
    auto skeleton_a = skeleton_a_container.Extract();

    SkeletonContainer<SkeletonMethodMoveSkeleton> skeleton_b_container{};
    skeleton_b_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "skeleton_method_move_semantics");
    auto skeleton_b = skeleton_b_container.Extract();

    // Step 2. Move-assign #1: Skeleton B = std::move(Skeleton A)  [before registering any handler]
    std::cout << "\nProvider: Step 2 - Move-assign #1: Skeleton B = std::move(Skeleton A)" << std::endl;
    skeleton_b = std::move(skeleton_a);

    // Step 3. Register Handler A on Skeleton B. Its body notifies handler_a_called when called.
    std::cout << "\nProvider: Step 3 - Register Handler A (a + b) on Skeleton B" << std::endl;
    concurrency::Notification handler_a_called{};
    RegisterHandlerOrFail(
        skeleton_b.moved_method_,
        [&handler_a_called](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a + b;
            handler_a_called.notify();
        },
        "Provider: Failed to register Handler A: ");

    // Step 4. Create Skeleton C, a second placeholder assignment target.
    std::cout << "\nProvider: Step 4 - Create placeholder Skeleton C" << std::endl;
    SkeletonContainer<SkeletonMethodMoveSkeleton> skeleton_c_container{};
    skeleton_c_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "skeleton_method_move_semantics");
    auto skeleton_c = skeleton_c_container.Extract();

    // Step 5. Move-assign #2: Skeleton C = std::move(Skeleton B)  [after registering the handler]
    std::cout << "\nProvider: Step 5 - Move-assign #2: Skeleton C = std::move(Skeleton B)" << std::endl;
    skeleton_c = std::move(skeleton_b);

    // Step 6. Offer Skeleton C
    std::cout << "\nProvider: Step 6 - Offer Skeleton C" << std::endl;
    OfferSkeletonOrFail(skeleton_c, "Provider: OfferService failed: ");

    // Step 7. Wait until Handler A has been called at least once.
    std::cout << "\nProvider: Step 7 - Wait for Handler A to be called" << std::endl;
    WaitOrFail(handler_a_called, stop_token, "Provider: Notification wait (Handler A) timed out");

    // Step 8. Register Handler C (a * b) on Skeleton C. Its body notifies
    //         handler_c_called when called.
    std::cout << "\nProvider: Step 8 - Register Handler C (a * b) on Skeleton C" << std::endl;
    concurrency::Notification handler_c_called{};
    RegisterHandlerOrFail(
        skeleton_c.moved_method_,
        [&handler_c_called](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a * b;
            handler_c_called.notify();
        },
        "Provider: Failed to register Handler C: ");

    // Step 9. Wait until Handler C has been called at least once before finishing.
    std::cout << "\nProvider: Step 9 - Wait for Handler C to be called" << std::endl;
    WaitOrFail(handler_c_called, stop_token, "Provider: Notification wait (Handler C) timed out");

    // Step 10. Return: Skeleton C's destructor calls StopOfferService().
    std::cout << "\nProvider: Step 10 - Finishing" << std::endl;
}

void RunProviderMoveAssignAfterOffered(const score::cpp::stop_token& stop_token)
{
    // Step 1. Create Skeleton A and Skeleton B.
    std::cout << "\nProvider: Step 1 - Create Skeleton A and placeholder Skeleton B" << std::endl;
    SkeletonContainer<SkeletonMethodMoveSkeleton> skeleton_a_container{};
    skeleton_a_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");
    auto skeleton_a = skeleton_a_container.Extract();

    SkeletonContainer<SkeletonMethodMoveSkeleton> skeleton_b_container{};
    skeleton_b_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "skeleton_method_move_semantics");
    auto skeleton_b = skeleton_b_container.Extract();

    // Step 2. Register Handler A on Skeleton A. Its body notifies notification_a when called.
    //         Register the placeholder handler on Skeleton B.
    std::cout << "\nProvider: Step 2 - Register Handler A (a + b) on Skeleton A" << std::endl;
    concurrency::Notification handler_a_called{};
    RegisterHandlerOrFail(
        skeleton_a.moved_method_,
        [&handler_a_called](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a + b;
            handler_a_called.notify();
        },
        "Provider: Failed to register Handler A: ");
    RegisterHandlerOrFail(
        skeleton_b.moved_method_,
        [](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a - b;
        },
        "Provider: Failed to register placeholder handler on Skeleton B: ");

    // Step 3. Offer both skeletons.
    std::cout << "\nProvider: Step 3 - Offer both skeletons" << std::endl;
    OfferSkeletonOrFail(skeleton_a, "Provider: OfferService (Skeleton A) failed: ");
    OfferSkeletonOrFail(skeleton_b, "Provider: OfferService (Skeleton B) failed: ");

    // Step 4. Wait until Handler A has been called at least once before proceeding.
    std::cout << "\nProvider: Step 4 - Wait for Handler A to be called" << std::endl;
    WaitOrFail(handler_a_called, stop_token, "Provider: Notification wait (Handler A) timed out");

    // Step 5. Sleep a random duration (0-500ms).
    SleepRandomDuration();

    // Step 6. Move-assign: Skeleton B = std::move(Skeleton A)  [already offered, random timing]
    std::cout << "\nProvider: Step 6 - Move-assign Skeleton B = std::move(Skeleton A)" << std::endl;
    skeleton_b = std::move(skeleton_a);

    // Step 7. Register Handler C (a * b) on Skeleton B. Its body notifies
    //         handler_c_called when called.
    std::cout << "\nProvider: Step 7 - Register Handler C (a * b) on Skeleton B" << std::endl;
    concurrency::Notification handler_c_called{};
    RegisterHandlerOrFail(
        skeleton_b.moved_method_,
        [&handler_c_called](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a * b;
            handler_c_called.notify();
        },
        "Provider: Failed to register Handler C: ");

    // Step 8. Wait until Handler C has been called at least once before finishing.
    std::cout << "\nProvider: Step 8 - Wait for Handler C to be called" << std::endl;
    WaitOrFail(handler_c_called, stop_token, "Provider: Notification wait (Handler C) timed out");

    // Step 9. Return: Skeleton B's destructor calls StopOfferService().
    std::cout << "\nProvider: Step 9 - Finishing" << std::endl;
}

}  // namespace

void RunProvider(const SkeletonMoveScenario& scenario, const score::cpp::stop_token& stop_token)
{
    switch (scenario)
    {
        case SkeletonMoveScenario::kMoveConstructBeforeOffered:
        {
            RunProviderMoveConstructBeforeOffered(stop_token);
            break;
        }
        case SkeletonMoveScenario::kMoveConstructAfterOffered:
        {
            RunProviderMoveConstructAfterOffered(stop_token);
            break;
        }
        case SkeletonMoveScenario::kMoveAssignBeforeOffered:
        {
            RunProviderMoveAssignBeforeOffered(stop_token);
            break;
        }
        case SkeletonMoveScenario::kMoveAssignAfterOffered:
        {
            RunProviderMoveAssignAfterOffered(stop_token);
            break;
        }
        case SkeletonMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
            FailTest("RunProvider: Unknown scenario");
    }
}

}  // namespace score::mw::com::test
