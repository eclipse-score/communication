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
#include "score/mw/com/test/move_semantics/skeleton_method/test_method_datatype.h"
#include "score/mw/com/test/move_semantics/skeleton_method/test_parameters.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <iostream>
#include <thread>

namespace score::mw::com::test
{
namespace
{

constexpr auto kDetectionWindow = std::chrono::milliseconds{5000};

constexpr auto kDetectionCallGap = std::chrono::milliseconds{5};

std::int32_t CallMethodOrFail(SkeletonMethodMoveProxy& proxy)
{
    auto result = proxy.moved_method_(kTestArgA, kTestArgB);
    if (!result.has_value())
    {
        FailTest("Consumer: moved_method_ call failed: ", result.error());
    }
    return *(result.value());
}

void VerifyResultIsOneOf(std::int32_t actual,
                         std::int32_t first_expected,
                         std::int32_t second_expected,
                         std::size_t call_index)
{
    if (actual != first_expected && actual != second_expected)
    {
        FailTest(
            "Consumer: call ", call_index, " expected ", first_expected, " or ", second_expected, " but got ", actual);
    }
}

}  // namespace

void RunConsumer(const SkeletonMoveScenario& scenario, const score::cpp::stop_token& stop_token)
{
    static_cast<void>(stop_token);

    // Step 1. Create proxy for kInstanceSpecifierMovedTo.
    std::cout << "\nConsumer: Step 1 - Create proxy (kMovedTo)" << std::endl;
    ProxyContainer<SkeletonMethodMoveProxy> proxy_moved_to_container{};
    proxy_moved_to_container.CreateProxy(kInstanceSpecifierMovedTo, "skeleton_method_move_semantics");
    auto& proxy_moved_to = proxy_moved_to_container.GetProxy();

    // Step 2. Keep calling the method until the return value identifies the second handler. For
    //         "before offered" scenarios the first call is always Handler A. For "after offered"
    //         (fuzzy) scenarios the move may already have happened, so any call may return either
    //         handler's result — each call is just checked against the two known-valid results.
    std::cout << "\nConsumer: Step 2 - Loop calling until second handler is detected" << std::endl;
    const std::int32_t first_expected = GetFirstHandlerExpectedResult(scenario);
    const std::int32_t second_expected = GetSecondHandlerExpectedResult(scenario);
    const auto deadline = std::chrono::steady_clock::now() + kDetectionWindow;
    std::size_t call_count = 0U;
    std::int32_t actual = 0;
    do
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            FailTest("Consumer: timed out waiting to observe the second handler's result");
        }
        actual = CallMethodOrFail(proxy_moved_to);
        VerifyResultIsOneOf(actual, first_expected, second_expected, call_count);
        ++call_count;
        std::this_thread::sleep_for(kDetectionCallGap);
    } while (actual != second_expected);

    std::cout << "\nConsumer: Step 2 done (" << call_count << " calls, second handler result=" << actual << ")"
              << std::endl;
}

}  // namespace score::mw::com::test
