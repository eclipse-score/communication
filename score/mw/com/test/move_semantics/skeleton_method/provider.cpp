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
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/move_semantics/skeleton_method/test_method_datatype.h"
#include "score/mw/com/test/move_semantics/skeleton_method/test_parameters.h"

#include <score/stop_token.hpp>

#include <iostream>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kProxyDoneShmPath{"/skeleton_method_move_semantics_proxy_done_sync"};
const std::string kSkeletonReadyShmPath{"/skeleton_method_move_semantics_skeleton_ready_sync"};

void RunProviderMoveConstructBeforeOffered(const score::cpp::stop_token& stop_token,
                                           ProcessSynchronizer& proxy_done_sync)
{
    // Step 1. Create Skeleton A
    std::cout << "\nProvider: Step 1 - Create Skeleton A" << std::endl;
    SkeletonContainer<SkeletonMethodMoveSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");
    auto skeleton_a = skeleton_container.Extract();

    // Step 2. Register Handler A on Skeleton A
    std::cout << "\nProvider: Step 2 - Register Handler A (a + b)" << std::endl;
    const auto register_result = skeleton_a.moved_method_.RegisterHandler(
        [](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a + b;
        });
    if (!register_result.has_value())
    {
        FailTest("Provider: Failed to register Handler A: ", register_result.error());
    }

    // Step 3. Move construct: Skeleton B = std::move(Skeleton A)  [before OfferService]
    std::cout << "\nProvider: Step 3 - Move construct Skeleton B = std::move(Skeleton A)" << std::endl;
    auto skeleton_b = std::move(skeleton_a);

    // Step 4. Offer Skeleton B
    std::cout << "\nProvider: Step 4 - Offer Skeleton B" << std::endl;
    const auto offer_result = skeleton_b.OfferService();
    if (!offer_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_result.error());
    }

    // Step 5. Wait for consumer to call Handler A and notify done
    std::cout << "\nProvider: Step 5 - Wait for consumer to call Handler A" << std::endl;
    if (!proxy_done_sync.WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort was stopped by stop_token");
    }
}

void RunProviderMoveConstructAfterOffered(const score::cpp::stop_token& stop_token,
                                          ProcessSynchronizer& proxy_done_sync,
                                          ProcessSynchronizer& skeleton_ready_sync)
{
    // Step 1. Create Skeleton A
    std::cout << "\nProvider: Step 1 - Create Skeleton A" << std::endl;
    SkeletonContainer<SkeletonMethodMoveSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");

    // Step 2. Register Handler A on Skeleton A
    std::cout << "\nProvider: Step 2 - Register Handler A (a + b)" << std::endl;
    const auto register_a_result = skeleton_container.GetSkeleton().moved_method_.RegisterHandler(
        [](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a + b;
        });
    if (!register_a_result.has_value())
    {
        FailTest("Provider: Failed to register Handler A: ", register_a_result.error());
    }

    // Step 3. Offer Skeleton A
    std::cout << "\nProvider: Step 3 - Offer Skeleton A" << std::endl;
    skeleton_container.OfferService("skeleton_method_move_semantics");

    // Step 4. Wait for proxy to be created (shared memory now exists)
    std::cout << "\nProvider: Step 4 - Wait for proxy created signal" << std::endl;
    if (!proxy_done_sync.WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (proxy created) was stopped by stop_token");
    }
    proxy_done_sync.Reset();

    // Step 5. Move construct: Skeleton B = std::move(Skeleton A)  [after SHM exists]
    std::cout << "\nProvider: Step 5 - Move construct Skeleton B = std::move(Skeleton A)" << std::endl;
    auto skeleton_b = std::move(skeleton_container.GetSkeleton());

    // Step 6. Signal consumer: move done, call Handler A
    std::cout << "\nProvider: Step 6 - Notify consumer: move done" << std::endl;
    skeleton_ready_sync.Notify();

    // Step 7. Wait for consumer to call Handler A
    std::cout << "\nProvider: Step 7 - Wait for Handler A call" << std::endl;
    if (!proxy_done_sync.WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (Handler A) was stopped by stop_token");
    }
    proxy_done_sync.Reset();

    // Step 8. Register Handler B on Skeleton B
    std::cout << "\nProvider: Step 8 - Register Handler B (a * b) on Skeleton B" << std::endl;
    const auto register_b_result = skeleton_b.moved_method_.RegisterHandler(
        [](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a * b;
        });
    if (!register_b_result.has_value())
    {
        FailTest("Provider: Failed to register Handler B: ", register_b_result.error());
    }

    // Step 9. Signal consumer: Handler B registered
    std::cout << "\nProvider: Step 9 - Notify consumer: Handler B registered" << std::endl;
    skeleton_ready_sync.Notify();

    // Step 10. Wait for consumer to call Handler B
    std::cout << "\nProvider: Step 10 - Wait for Handler B call" << std::endl;
    if (!proxy_done_sync.WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (Handler B) was stopped by stop_token");
    }
}

void RunProviderMoveAssignBeforeOffered(const score::cpp::stop_token& stop_token, ProcessSynchronizer& proxy_done_sync)
{
    // Step 1. Create moved_from_skeleton (kMovedTo identity) and moved_to_skeleton (kMovedFrom identity)
    std::cout << "\nProvider: Step 1 - Create two skeletons" << std::endl;
    SkeletonContainer<SkeletonMethodMoveSkeleton> moved_from_container{};
    moved_from_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");
    auto moved_from_skeleton = moved_from_container.Extract();

    SkeletonContainer<SkeletonMethodMoveSkeleton> moved_to_container{};
    moved_to_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "skeleton_method_move_semantics");
    auto moved_to_skeleton = moved_to_container.Extract();

    // Step 2. Register Handler A on moved_from_skeleton
    std::cout << "\nProvider: Step 2 - Register Handler A (a + b) on moved_from_skeleton" << std::endl;
    const auto register_result = moved_from_skeleton.moved_method_.RegisterHandler(
        [](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a + b;
        });
    if (!register_result.has_value())
    {
        FailTest("Provider: Failed to register Handler A: ", register_result.error());
    }

    // Step 3. Move assign: moved_to_skeleton = std::move(moved_from_skeleton)
    //         moved_to_skeleton now has kMovedTo identity + Handler A
    std::cout << "\nProvider: Step 3 - Move assign moved_to = std::move(moved_from)" << std::endl;
    moved_to_skeleton = std::move(moved_from_skeleton);

    // Step 4. Offer moved_to_skeleton
    std::cout << "\nProvider: Step 4 - Offer moved_to_skeleton" << std::endl;
    const auto offer_result = moved_to_skeleton.OfferService();
    if (!offer_result.has_value())
    {
        FailTest("Provider: OfferService failed: ", offer_result.error());
    }

    // Step 5. Wait for consumer to call Handler A and notify done
    std::cout << "\nProvider: Step 5 - Wait for consumer to call Handler A" << std::endl;
    if (!proxy_done_sync.WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort was stopped by stop_token");
    }
}

void RunProviderMoveAssignAfterOffered(const score::cpp::stop_token& stop_token,
                                       ProcessSynchronizer& proxy_done_sync,
                                       ProcessSynchronizer& skeleton_ready_sync)
{
    // Step 1. Create two skeletons
    std::cout << "\nProvider: Step 1 - Create two skeletons" << std::endl;
    SkeletonContainer<SkeletonMethodMoveSkeleton> moved_from_container{};
    moved_from_container.CreateSkeleton(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");

    SkeletonContainer<SkeletonMethodMoveSkeleton> moved_to_container{};
    moved_to_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "skeleton_method_move_semantics");

    // Step 2. Register Handler A on moved_from, Handler B on moved_to
    std::cout << "\nProvider: Step 2 - Register Handler A (a + b) and Handler B (a * b)" << std::endl;
    const auto register_a_result = moved_from_container.GetSkeleton().moved_method_.RegisterHandler(
        [](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a + b;
        });
    if (!register_a_result.has_value())
    {
        FailTest("Provider: Failed to register Handler A: ", register_a_result.error());
    }

    const auto register_b_result = moved_to_container.GetSkeleton().moved_method_.RegisterHandler(
        [](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a * b;
        });
    if (!register_b_result.has_value())
    {
        FailTest("Provider: Failed to register Handler B: ", register_b_result.error());
    }

    // Step 3. Offer both skeletons
    std::cout << "\nProvider: Step 3 - Offer both skeletons" << std::endl;
    moved_from_container.OfferService("skeleton_method_move_semantics");
    moved_to_container.OfferService("skeleton_method_move_semantics");

    // Step 4. Wait for both proxies to be created (single notify from consumer)
    std::cout << "\nProvider: Step 4 - Wait for both proxies created signal" << std::endl;
    if (!proxy_done_sync.WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (proxies created) was stopped by stop_token");
    }
    proxy_done_sync.Reset();

    // Step 5. Move assign: moved_to = std::move(moved_from)
    //         moved_to now has kMovedTo identity + Handler A
    std::cout << "\nProvider: Step 5 - Move assign moved_to = std::move(moved_from)" << std::endl;
    auto moved_from_skeleton = moved_from_container.Extract();
    auto moved_to_skeleton = moved_to_container.Extract();
    moved_to_skeleton = std::move(moved_from_skeleton);

    // Step 6. Signal consumer: move done, call Handler A via kMovedTo proxy
    std::cout << "\nProvider: Step 6 - Notify consumer: move done" << std::endl;
    skeleton_ready_sync.Notify();

    // Step 7. Wait for consumer to call Handler A (a+b = 15)
    std::cout << "\nProvider: Step 7 - Wait for Handler A call" << std::endl;
    if (!proxy_done_sync.WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (Handler A) was stopped by stop_token");
    }
    proxy_done_sync.Reset();

    // Step 8. Register Handler C on moved_to_skeleton (Skeleton A internally stores B's handle)
    std::cout << "\nProvider: Step 8 - Register Handler C (a - b) on moved_to_skeleton" << std::endl;
    const auto register_c_result = moved_to_skeleton.moved_method_.RegisterHandler(
        [](std::int32_t& result, const std::int32_t& a, const std::int32_t& b) {
            result = a - b;
        });
    if (!register_c_result.has_value())
    {
        FailTest("Provider: Failed to register Handler C: ", register_c_result.error());
    }

    // Step 9. Signal consumer: Handler C registered
    std::cout << "\nProvider: Step 9 - Notify consumer: Handler C registered" << std::endl;
    skeleton_ready_sync.Notify();

    // Step 10. Wait for consumer to call Handler C (a-b = 5)
    std::cout << "\nProvider: Step 10 - Wait for Handler C call" << std::endl;
    if (!proxy_done_sync.WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (Handler C) was stopped by stop_token");
    }
}

}  // namespace

void RunProvider(const SkeletonMoveScenario& scenario, const score::cpp::stop_token& stop_token)
{
    auto proxy_done_sync = ProcessSynchronizer::Create(kProxyDoneShmPath);
    if (!proxy_done_sync.has_value())
    {
        FailTest("Provider: Failed to create proxy_done ProcessSynchronizer");
    }

    switch (scenario)
    {
        case SkeletonMoveScenario::kMoveConstructBeforeOffered:
        {
            RunProviderMoveConstructBeforeOffered(stop_token, proxy_done_sync.value());
            break;
        }
        case SkeletonMoveScenario::kMoveConstructAfterOffered:
        {
            auto skeleton_ready_sync = ProcessSynchronizer::Create(kSkeletonReadyShmPath);
            if (!skeleton_ready_sync.has_value())
            {
                FailTest("Provider: Failed to create skeleton_ready ProcessSynchronizer");
            }
            RunProviderMoveConstructAfterOffered(stop_token, proxy_done_sync.value(), skeleton_ready_sync.value());
            break;
        }
        case SkeletonMoveScenario::kMoveAssignBeforeOffered:
        {
            RunProviderMoveAssignBeforeOffered(stop_token, proxy_done_sync.value());
            break;
        }
        case SkeletonMoveScenario::kMoveAssignAfterOffered:
        {
            auto skeleton_ready_sync = ProcessSynchronizer::Create(kSkeletonReadyShmPath);
            if (!skeleton_ready_sync.has_value())
            {
                FailTest("Provider: Failed to create skeleton_ready ProcessSynchronizer");
            }
            RunProviderMoveAssignAfterOffered(stop_token, proxy_done_sync.value(), skeleton_ready_sync.value());
            break;
        }
        case SkeletonMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
        {
            FailTest("Provider: Unknown scenario");
            break;
        }
    }
}

}  // namespace score::mw::com::test
