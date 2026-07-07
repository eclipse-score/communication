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
#include "score/mw/com/test/move_semantics/skeleton_method/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/move_semantics/skeleton_method/test_method_datatype.h"
#include "score/mw/com/test/move_semantics/skeleton_method/test_parameters.h"

#include <score/stop_token.hpp>

#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kProxyDoneShmPath{"/skeleton_method_move_semantics_proxy_done_sync"};
const std::string kSkeletonReadyShmPath{"/skeleton_method_move_semantics_skeleton_ready_sync"};

}  // namespace

void RunConsumer(const SkeletonMoveScenario& scenario, const score::cpp::stop_token& stop_token)
{
    auto proxy_done_sync = ProcessSynchronizer::Create(kProxyDoneShmPath);
    if (!proxy_done_sync.has_value())
    {
        FailTest("Consumer: Failed to create proxy_done ProcessSynchronizer");
    }

    // Safety guard: always notify provider on exit so it does not wait indefinitely on failure
    ExitFunctionGuard done_guard{[&proxy_done_sync]() {
        proxy_done_sync->Notify();
    }};

    // Step 1. Create proxy for kInstanceSpecifierMovedTo
    std::cout << "\nConsumer: Step 1 - Create proxy (kMovedTo)" << std::endl;
    ProxyContainer<SkeletonMethodMoveProxy> proxy_moved_to_container{};
    proxy_moved_to_container.CreateProxy(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");
    auto& proxy_moved_to = proxy_moved_to_container.GetProxy();

    // Step 2. For kMoveAssignAfterOffered: also create proxy for kInstanceSpecifierMovedFrom
    //         (both skeletons are offered; creating the proxy confirms SHM exists for both)
    std::optional<ProxyContainer<SkeletonMethodMoveProxy>> proxy_moved_from_container{};
    if (scenario == SkeletonMoveScenario::kMoveAssignAfterOffered)
    {
        std::cout << "\nConsumer: Step 2 - Create proxy (kMovedFrom)" << std::endl;
        proxy_moved_from_container.emplace();
        proxy_moved_from_container->CreateProxy(kInstanceSpecifierMovedFrom, "skeleton_method_move_semantics");
    }

    // Step 3. For after-offered scenarios: signal provider that proxy/proxies are created
    if (IsAfterOffered(scenario))
    {
        std::cout << "\nConsumer: Step 3 - Notify provider: proxy/proxies created" << std::endl;
        proxy_done_sync->Notify();
    }

    // Step 4. Create skeleton_ready synchronizer for after-offered scenarios
    std::unique_ptr<ProcessSynchronizer> skeleton_ready_sync{};
    if (IsAfterOffered(scenario))
    {
        skeleton_ready_sync = ProcessSynchronizer::CreateUniquePtr(kSkeletonReadyShmPath);
    }

    // Step 5. Call loop: one iteration for before-offered, two for after-offered
    const std::size_t num_iterations = GetNumberOfCallIterations(scenario);
    for (std::size_t iteration = 0U; iteration < num_iterations; ++iteration)
    {
        std::cout << "\nConsumer: Iteration " << (iteration + 1U) << " of " << num_iterations << std::endl;

        // For after-offered scenarios: wait for provider to signal ready
        // iter 0 → move complete; iter 1 → next handler registered
        if (IsAfterOffered(scenario))
        {
            std::cout << "\nConsumer: Wait for skeleton_ready signal" << std::endl;
            if (!skeleton_ready_sync->WaitWithAbort(stop_token))
            {
                FailTest("Consumer: skeleton_ready WaitWithAbort was stopped by stop_token");
            }
            skeleton_ready_sync->Reset();
        }

        //  Call the method
        std::cout << "\nConsumer: Calling moved_method_(" << kTestArgA << ", " << kTestArgB << ")" << std::endl;
        auto result = proxy_moved_to.moved_method_(kTestArgA, kTestArgB);
        if (!result.has_value())
        {
            FailTest("Consumer: moved_method_ call failed: ", result.error());
        }
        const std::int32_t actual = *(result.value());

        // Verify result.
        const std::int32_t expected = GetExpectedResult(scenario, iteration);
        if (actual != expected)
        {
            FailTest("Consumer: iteration ", iteration, " expected ", expected, " but got ", actual);
        }
        std::cout << "\nConsumer: Iteration " << (iteration + 1U) << " passed (result=" << actual << ")" << std::endl;

        // Notify provider: call done
        proxy_done_sync->Notify();
    }

    std::cout << "\nConsumer: All iterations done" << std::endl;
}

}  // namespace score::mw::com::test
