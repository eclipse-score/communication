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

/// \brief Overall bound for how long the consumer tolerates method call failures before giving up.
/// \details The provider may transiently stop offering the service while re-registering a method handler, since
/// impl::SkeletonMethod::RegisterHandler() must not be called while the service is offered. During that short
/// window, method calls are expected to fail; such failures are treated as transient and retried. The
/// stop-offer/RegisterHandler/offer sequence itself is a fast, local, synchronous operation, so a generous but
/// bounded margin of a couple of seconds is sufficient to absorb scheduling jitter without masking real failures.
constexpr auto kMaxTransientFailureDuration = std::chrono::seconds{2};

std::optional<std::int32_t> TryCallMethod(SkeletonMethodMoveProxy& proxy)
{
    auto result = proxy.moved_method_(kTestArgA, kTestArgB);
    if (!result.has_value())
    {
        return std::nullopt;
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
    //         Calls may transiently fail while the provider stop-offers and re-offers the service in order to
    //         (re-)register a handler; such failures are retried until kMaxTransientFailureDuration elapses.
    std::cout << "\nConsumer: Step 2 - Loop calling until second handler is detected" << std::endl;
    std::size_t call_count = 0U;
    std::optional<std::int32_t> actual_method_return_value{};
    const auto transient_failure_deadline = std::chrono::steady_clock::now() + kMaxTransientFailureDuration;
    while (!actual_method_return_value.has_value() ||
           actual_method_return_value.value() != kSecondHandlerExpectedResult)
    {
        const auto call_result = TryCallMethod(proxy_moved_to);
        ++call_count;

        if (!call_result.has_value())
        {
            if (std::chrono::steady_clock::now() > transient_failure_deadline)
            {
                FailTest(
                    "Consumer: method calls kept failing for more than ", kMaxTransientFailureDuration.count(), "s");
            }
            std::this_thread::sleep_for(kMethodCallInterval);
            continue;
        }
        actual_method_return_value = call_result;

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
