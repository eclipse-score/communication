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
#include "score/mw/com/test/methods/multiple_proxies/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/methods_test_resources/skeleton_container.h"
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"
#include "score/mw/com/test/methods/multiple_proxies/duplicate_signatures_datatype.h"
#include "score/mw/com/types.h"

#include "score/result/result.h"

#include <score/stop_token.hpp>

#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{
const std::string kInterprocessNotificationShmPath{"/multiple_proxies_test_interprocess_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"multiple_proxies/MultiMethodProvider"}).value();

std::array<std::atomic<std::uint16_t>, kNumRegisteredMethods> gMethodCallCounts{0U, 0U, 0U, 0U, 0U};
std::array<std::atomic<std::uint16_t>, kNumRegisteredMethods> gConcurrentCalls{0U, 0U, 0U, 0U, 0U};
std::array<std::atomic<std::uint16_t>, kNumRegisteredMethods> gOccuranceOfConcurrentCalls{0U, 0U, 0U, 0U, 0U};

using InpArgType = DuplicateSignatureSkeleton::InpArgType;
using ReturnType = DuplicateSignatureSkeleton::ReturnType;

auto HandlerMaker(InpArgType idx)
{
    return [idx](InpArgType val) -> ReturnType {
        const auto count = gMethodCallCounts.at(idx).fetch_add(1) + 1;
        auto current_call_numner = gConcurrentCalls.at(idx).fetch_add(1);
        if (current_call_numner > 0)
        {
            gOccuranceOfConcurrentCalls.at(idx).fetch_add(1);
        }

        // Simulate some processing work to increase the time window for concurrent execution
        // This makes it more likely that multiple handlers will be executing at the same time
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        std::cout << "Provider: method" << idx << " called (count=" << count << ") with val=" << val << '\n';
        gConcurrentCalls.at(idx).fetch_add(-1);
        return val * (kMethodResultMultiplierBase + idx);
    };
}

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token,
                  const std::vector<std::size_t>& expected_method_call_counts,
                  std::size_t num_consumers)
{
    std::vector<std::unique_ptr<ProcessSynchronizer>> process_synchronizers;
    for (std::size_t i{0}; i < num_consumers; ++i)
    {
        process_synchronizers.emplace_back(
            ProcessSynchronizer::CreateUniquePtr(CreateInterprocessNotificationShmPath(i)));

        std::cout << "provider sync: " << CreateInterprocessNotificationShmPath(i) << std::endl;
    }

    SkeletonContainer<DuplicateSignatureSkeleton> skeleton_container{};

    std::cout << "\nProvider: Step 1. Create skeleton" << std::endl;
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "multiple_proxies");
    auto& skeleton = skeleton_container.GetSkeleton();

    std::cout << "Provider: Ready for method calls from multiple proxies\n";

    std::cout << "\nProvider: Step 2. Register method handlers" << std::endl;
    auto register_handler_or_fail = [](auto& method, std::size_t method_idx) {
        if (!method.RegisterHandler(HandlerMaker(method_idx)))
        {
            FailTest("Methods multiple_proxies provider failed: Could not register handler for method ", method_idx);
        }
    };
    register_handler_or_fail(skeleton.method0, 0U);
    register_handler_or_fail(skeleton.method1, 1U);
    register_handler_or_fail(skeleton.method2, 2U);
    register_handler_or_fail(skeleton.method3, 3U);
    register_handler_or_fail(skeleton.method4, 4U);

    std::cout << "\nProvider: Step 3. Offer service" << std::endl;
    skeleton_container.OfferService("multiple_proxies");

    std::cout << "\nProvider: Step 4. Wait for consumers" << std::endl;
    for (std::size_t i{0}; i < process_synchronizers.size(); ++i)
    {
        std::cout << "Provider: Waiting for Consumer" << i << " to finish...\n";
        if (!process_synchronizers.at(i)->WaitWithAbort(stop_token))
        {
            FailTest("Provider: WaitWithAbort for Consumer", i, " was stopped by stop_token\n");
        }
    }
    std::cout << "Provider: All consumers have finished...\n\n";

    std::cout << "\nProvider: Step 5. Verify the number of method calls are correct" << std::endl;
    std::size_t idx{0};
    for (const auto& method_call_count : gMethodCallCounts)
    {
        const auto actual_method_call_count = method_call_count.load();
        const auto expected_method_call_count = expected_method_call_counts.at(idx);

        std::cout << "Recorded number of concurrent calls for idx: " << idx << "\n";
        std::cout << "Expected: " << expected_method_call_count << ". Actual: " << actual_method_call_count << "\n";
        if (expected_method_call_count != actual_method_call_count)
        {
            std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
            std::cout << "Provider: method called unexpected amount of times.\n";
            std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
            FailTest();
        }
        idx++;
    }
    std::cout << "Provider: Shutting down.\n";
}

}  // namespace score::mw::com::test
