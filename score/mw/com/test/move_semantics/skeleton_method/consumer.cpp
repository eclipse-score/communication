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

#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

namespace score::mw::com::test
{
namespace
{

constexpr auto kMethodCallInterval = std::chrono::milliseconds{2};

std::int32_t CallMethodOrFail(SkeletonMethodMoveProxy& proxy)
{
    auto result = proxy.moved_method_(kTestArgA, kTestArgB);
    if (!result.has_value())
    {
        FailTest("Consumer: moved_method_ call failed: ", result.error());
    }
    return *(result.value());
}

}  // namespace

void RunConsumer()
{
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
    std::size_t call_count = 0U;
    std::optional<std::int32_t> actual_method_return_value{};
    while (!actual_method_return_value.has_value() ||
           actual_method_return_value.value() != kSecondHandlerExpectedResult)
    {
        actual_method_return_value = CallMethodOrFail(proxy_moved_to);
        ++call_count;

        // If the first handler is still registered, then we continue calling the method.
        if (actual_method_return_value == kFirstHandlerExpectedResult)
        {
            std::this_thread::sleep_for(kMethodCallInterval);
            continue;
        }

        // If the second handler has been registered, then we can finish.
        if (actual_method_return_value == kSecondHandlerExpectedResult)
        {
            break;
        }

        FailTest("Consumer: call ",
                 call_count,
                 " expected ",
                 kFirstHandlerExpectedResult,
                 " or ",
                 kSecondHandlerExpectedResult,
                 " but got ",
                 actual_method_return_value.value());
    }

    std::cout << "\nConsumer: Step 2 done (" << call_count
              << " calls, second handler result=" << actual_method_return_value.value() << ")" << std::endl;
}

}  // namespace score::mw::com::test
