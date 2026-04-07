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
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/stop_offer_during_call/test_method_datatype.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

namespace
{
using namespace score::mw::com;
using namespace score::mw::com::test;

const std::string_view kInstanceSpecifierSV = "test/methods/StopOfferTest";
void check_result(std::future<score::Result<MethodReturnTypePtr<std::int32_t>>>&& result_future,
                  std::int32_t expected_value)
{
    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    auto result = std::move(result_future).get();
    std::cout << "Async call completed, checking result...\n";
    // FailTestIf(!result.has_value(), "FAIL: Method call failed with error unexpectedly: ", result.error());
    if (!result.has_value())
    {
        FailTest("FAIL: Method call failed with error unexpectedly: ", result.error());
    }

    auto& return_value_ptr = result.value();

    FailTestIf(return_value_ptr.get() == nullptr,
                 " FAIL: Method call returned null pointer, instead of a valid ptr to shm.");

    // clang-format off
    FailTestIf(*return_value_ptr != expected_value,
                 "FAIL: Method call returned wrong value. Expected: ", expected_value, ", Actual: ", *return_value_ptr);
    // clang-format on

    std::cout << "Method call succeeded with expected value: " << *return_value_ptr << std::endl;
}
struct ExecutionMarkers
{
    std::atomic<bool> method_started{false};
    std::atomic<bool> green_light_for_completion{false};
    std::atomic<bool> method_completed{false};
};

void ResetExecutionMarkers(ExecutionMarkers& markers)
{
    markers.method_started = false;
    markers.green_light_for_completion = false;
    markers.method_completed = false;
}

}  // namespace

int main(int argc, const char** argv)
{
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    ExecutionMarkers execution_markers{};

    std::cout << "Step 1: Create skeleton with long-running handler." << std::endl;

    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierSV});
    if (!instance_specifier_result.has_value())
    {
        std::cout << "Could not create Instance specifier!.\n"
                  << "Here is why: " << instance_specifier_result.error() << "\n\n"
                  << std::flush;
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    auto skeleton_result = StopOfferDuringCallSkeleton::Create(instance_specifier);

    if (!skeleton_result.has_value())
    {
        std::cerr << "Failed to create skeleton: " << skeleton_result.error() << std::endl;
        return EXIT_FAILURE;
    }
    auto skeleton = std::make_unique<StopOfferDuringCallSkeleton>(std::move(skeleton_result).value());
    std::cout << "Step 1: skeleton created." << std::endl;

    auto handler = [&execution_markers](std::int32_t input) -> std::int32_t {
        execution_markers.method_started = true;
        constexpr auto idling_time = std::chrono::milliseconds(1);
        while (!execution_markers.green_light_for_completion)
        {
            std::this_thread::sleep_for(idling_time);
        }
        std::cout << "Consumer: green light was given... Continuing to completion...\n";
        execution_markers.method_completed = true;
        return input * 2;
    };

    std::cout << "Step 1.1: Registering handler." << std::endl;
    auto register_result = skeleton->long_running_method.RegisterHandler(std::move(handler));
    if (!register_result.has_value())
    {
        std::cerr << "Failed to register handler: " << register_result.error() << std::endl;
        return EXIT_FAILURE;
    }

    auto offer_result = skeleton->OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Failed to offer service: " << offer_result.error() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Step 1.2: Service offered" << std::endl;

    std::cout << "Step 2: Creating Proxy" << std::endl;
    auto find_result = StopOfferDuringCallProxy::FindService(instance_specifier);
    if (!find_result.has_value() || find_result->size() != 1)
    {
        std::cerr << "FindService failed or returned wrong count" << std::endl;
        return EXIT_FAILURE;
    }

    auto proxy_result = StopOfferDuringCallProxy::Create((*find_result)[0], {"long_running_method"});
    if (!proxy_result.has_value())
    {
        std::cerr << "Failed to create proxy: " << proxy_result.error() << std::endl;
        return EXIT_FAILURE;
    }
    auto proxy = std::make_unique<StopOfferDuringCallProxy>(std::move(proxy_result).value());

    std::cout << "Step 2.1: Call the long running method asynchronously\n";
    constexpr std::int32_t test_input = 12;
    constexpr std::int32_t expected_output = 24;

    constexpr std::size_t max_repetition_count{5};
    std::cout
        << "Step 3: Starting test loop. call mathod, wait for it to start, call StopOfferService, check result. Repeat "
        << max_repetition_count << " times.\n";
    std::size_t repetiction_count{0};
    while (repetiction_count < max_repetition_count)
    {

        auto async_result =
            std::async(std::launch::async, [&proxy, test_input]() -> score::Result<MethodReturnTypePtr<std::int32_t>> {
                auto result = proxy->long_running_method(test_input);
                return result;
            });

        std::cout << "Step 3.1 : Wait for handler to start, then call StopOfferService\n" << std::flush;
        while (!execution_markers.method_started)
        {
            constexpr auto idling_time = std::chrono::microseconds(50);
            std::this_thread::sleep_for(idling_time);
        }

        if (execution_markers.method_completed)
        {
            std::cout << " Step 3.2: StopOfferService was not calld since the method was already completed\n"
                      << std::flush;
            continue;
        }

        // green_light_for_completion needs to be set before calling StopOfferService, otherwise we will get a
        // deadlock, since StopOfferService allways waits for the completion of the method calls before it
        // completes.
        execution_markers.green_light_for_completion = true;
        skeleton->StopOfferService();
        std::cout << " Step 3.2: StopOfferService was called while the method was still running\n" << std::flush;

        ++repetiction_count;
        check_result(std::move(async_result), expected_output);
        ResetExecutionMarkers(execution_markers);
    }

    // Test passes if:
    // - No crash occurred
    // - StopOfferService didn't hang indefinitely
    // - Method call either succeeded or returned an error (not hung)

    if (repetiction_count == 0)
    {
        std::cerr << "FAIL: StopOfferService was never called during method execution" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
