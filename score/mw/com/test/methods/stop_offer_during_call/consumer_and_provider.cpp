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
#include "score/mw/com/test/methods/stop_offer_during_call/consumer_and_provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/stop_offer_during_call/test_method_datatype.h"

#include "score/concurrency/notification.h"

#include "score/mw/com/types.h"

#include <chrono>
#include <cstddef>
#include <future>
#include <iostream>
#include <string_view>
#include <thread>

namespace score::mw::com::test
{
namespace
{
const std::string_view kInstanceSpecifierSV = "test/methods/StopOfferTest";

void OfferService(StopOfferDuringCallSkeleton& skeleton)
{
    auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        FailTest("Could not offer service: ", offer_result.error());
    }
    std::cout << "Service offered successfully\n";
}

auto FindService(InstanceSpecifier& instance_specifier) -> StopOfferDuringCallProxy
{

    auto find_result = StopOfferDuringCallProxy::FindService(instance_specifier);
    if (!find_result.has_value())
    {
        FailTest("FindService failed. Error: ", find_result.error());
    }

    if (find_result->size() != 1)
    {
        FailTest("FindService returned wrong number of handles. Expected 1, Received: ", find_result->size());
    }

    auto proxy_result = StopOfferDuringCallProxy::Create((*find_result)[0]);
    if (!proxy_result.has_value())
    {
        FailTest("Failed to create proxy: ", proxy_result.error());
    }
    auto proxy = std::move(proxy_result).value();
    return proxy;
}

class ExecutionMarkers
{
  public:
    void Reset()
    {
        method_started.reset();
        green_light_for_completion.reset();
        method_completed = false;
    }

    score::concurrency::Notification method_started{};
    score::concurrency::Notification green_light_for_completion{};
    bool method_completed{};
};

}  // namespace

/// Test passes if:
/// - No crash occurred
/// - StopOfferService didn't hang indefinitely
/// - Method call succeeded
void RunConsumerAndProvider(const score::cpp::stop_token& stop_token)
{
    ExecutionMarkers execution_markers{};

    std::cout << "Step 1: Create skeleton with long-running handler." << std::endl;

    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierSV});
    if (!instance_specifier_result.has_value())
    {
        FailTest("Could not create Instance specifier: ", instance_specifier_result.error());
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    auto skeleton_result = StopOfferDuringCallSkeleton::Create(instance_specifier);

    if (!skeleton_result.has_value())
    {
        FailTest("Failed to create skeleton: ", skeleton_result.error());
    }

    auto skeleton = std::move(skeleton_result).value();
    std::cout << "Step 2.1: skeleton created." << std::endl;

    auto handler = [&execution_markers, &stop_token](std::int32_t& return_value, const std::int32_t& input) {
        execution_markers.method_started.notify();
        execution_markers.green_light_for_completion.waitWithAbort(stop_token);
        std::cout << "Consumer: green light was given... Continuing to completion...\n";
        execution_markers.method_completed = true;
        return_value = input * 2;
    };

    std::cout << "Step 2.2: Registering handler." << std::endl;
    auto register_result = skeleton.long_running_method.RegisterHandler(std::move(handler));
    if (!register_result.has_value())
    {
        FailTest("Failed to register handler: ", register_result.error());
    }

    OfferService(skeleton);

    std::cout << "Step 2.3: Creating a proxy and finding reoffered service" << std::endl;
    auto proxy = FindService(instance_specifier);

    std::cout << "Step 3: Call the long running method asynchronously\n";
    constexpr std::int32_t test_input = 12;
    constexpr std::int32_t expected_output = 24;

    constexpr std::size_t max_repetition_count{5};
    std::cout
        << "Step 4: Starting test loop. Call method, wait for it to start, call StopOfferService, check result. Repeat "
        << max_repetition_count << " times.\n";
    std::size_t successful_repetition_count{0};
    while (successful_repetition_count < max_repetition_count)
    {
        std::cout << "Step 4.1: (Re)Offering the service\n" << std::flush;

        auto async_result =
            std::async(std::launch::async, [&proxy, &stop_token, test_input, &expected_output]() -> bool {
                while (!stop_token.stop_requested())
                {
                    // this loop is required since the proxy might not yet be resubscribed to the reoffered
                    // service
                    auto result = proxy.long_running_method(test_input);
                    if (result.has_value())
                    {
                        auto& return_value_ptr = result.value();
                        if (return_value_ptr.get() == nullptr)
                        {
                            FailTest(" FAIL: Method call returned null pointer, instead of a valid ptr to shm.");
                        }

                        if (*return_value_ptr != expected_output)
                        {
                            FailTest("FAIL: Method call returned wrong value. Expected: ",
                                     expected_output,
                                     ", Actual: ",
                                     *return_value_ptr);
                        }
                        return true;
                    }
                    auto idling_time = std::chrono::milliseconds(1);
                    std::this_thread::sleep_for(idling_time);
                }
                return false;
            });

        std::cout << "Step 4.2: Wait for handler to start, then call StopOfferService" << std::endl;
        execution_markers.method_started.waitWithAbort(stop_token);

        if (execution_markers.method_completed)
        {
            std::cout << "Step 4.3: StopOfferService was not called since the method was already completed"
                      << std::endl;
            continue;
        }

        // green_light_for_completion needs to be set before calling StopOfferService, otherwise we will get a
        // deadlock, since StopOfferService always waits for the completion of the method calls before it
        // completes.
        execution_markers.green_light_for_completion.notify();
        skeleton.StopOfferService();
        std::cout << "Step 4.4: StopOfferService was called while the method was still running" << std::endl;

        ++successful_repetition_count;
        execution_markers.Reset();
        OfferService(skeleton);
    }

    if (successful_repetition_count == 0)
    {
        FailTest("FAIL: StopOfferService was never called during method execution");
    }
}

}  // namespace score::mw::com::test
