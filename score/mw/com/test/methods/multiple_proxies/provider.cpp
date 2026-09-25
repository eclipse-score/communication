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
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"
#include "score/mw/com/test/methods/multiple_proxies/duplicate_signatures_datatype.h"
#include "score/mw/com/types.h"

#include "score/result/result.h"

#include <score/stop_token.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/multiple_proxies_test_interprocess_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"multiple_proxies/MultiMethodProvider"}).value();

using InputArgType = DuplicateSignatureSkeleton::InputArgType;
using CallCountType = DuplicateSignatureSkeleton::CallIndexType;
using ConsumerIdType = DuplicateSignatureSkeleton::ConsumerIdType;
using ProxyIdType = DuplicateSignatureSkeleton::ProxyIdType;
using ReturnType = DuplicateSignatureSkeleton::ReturnType;

class MethodCallCounter
{
  public:
    MethodCallCounter(std::size_t num_consumers, std::size_t num_proxies_per_consumer) : counts_(num_consumers)
    {
        for (auto& method_call_counts_per_proxy : counts_)
        {
            method_call_counts_per_proxy.reserve(num_proxies_per_consumer);
            for (std::size_t proxy_id{0U}; proxy_id < num_proxies_per_consumer; ++proxy_id)
            {
                method_call_counts_per_proxy.emplace_back(std::make_unique<MethodCallCountsPerMethod>());
            }
        }
    }

    std::size_t Increment(std::size_t consumer_id, std::size_t proxy_id, std::size_t method_id)
    {
        auto& method_count = counts_.at(consumer_id).at(proxy_id)->at(method_id);
        return method_count.fetch_add(1U) + 1U;
    }

    std::size_t Get(std::size_t consumer_id, std::size_t proxy_id, std::size_t method_id) const
    {
        return counts_.at(consumer_id).at(proxy_id)->at(method_id).load();
    }

  private:
    using MethodCallCountsPerMethod = std::array<std::atomic<std::size_t>, kNumRegisteredMethods>;
    using MethodCallCountsPerProxy = std::vector<std::unique_ptr<MethodCallCountsPerMethod>>;
    using MethodCallCountsPerConsumer = std::vector<MethodCallCountsPerProxy>;

    MethodCallCountsPerConsumer counts_;
};

auto HandlerMaker(MethodCallCounter& method_call_counter, std::size_t method_idx)
{
    return [method_idx, &method_call_counter](ReturnType& return_value,
                                              const InputArgType& sent_value,
                                              const CallCountType& call_count,
                                              const ConsumerIdType& consumer_id,
                                              const ProxyIdType& proxy_id) {
        const auto count = method_call_counter.Increment(consumer_id, proxy_id, method_idx);

        if (count != call_count)
        {
            FailTest("Provider: Call count argument does not match expected value. Expected: ",
                     count,
                     ", Actual: ",
                     call_count,
                     " for Consumer ",
                     consumer_id,
                     ", Proxy ",
                     proxy_id,
                     ", method",
                     method_idx,
                     '\n');
        }

        // Simulate some processing work to increase the time window for concurrent execution
        // This makes it more likely that multiple handlers will be executing at the same time
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // clang-format off
        PrintLine("Provider: Consumer process ", consumer_id, ", Proxy ", proxy_id, ", method", method_idx,
                  " called (skel count=", count, ") with val=", sent_value, " and proxy count=", call_count, '\n');
        // clang-format on

        return_value = ReturnType{sent_value, call_count, consumer_id, proxy_id};
    };
}

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token,
                  std::size_t num_consumers,
                  std::size_t num_proxies_per_consumer,
                  std::size_t num_method_calls_per_proxy)
{
    MethodCallCounter method_call_counter{num_consumers, num_proxies_per_consumer};

    std::vector<std::unique_ptr<ProcessSynchronizer>> process_synchronizers;
    for (std::size_t i{0}; i < num_consumers; ++i)
    {
        process_synchronizers.emplace_back(
            ProcessSynchronizer::CreateUniquePtr(CreateInterprocessNotificationShmPath(i)));
    }

    SkeletonContainer<DuplicateSignatureSkeleton> skeleton_container{};

    std::cout << "\nProvider: Step 1. Create skeleton" << std::endl;
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "multiple_proxies");
    auto& skeleton = skeleton_container.GetSkeleton();

    std::cout << "Provider: Ready for method calls from multiple proxies\n";

    std::cout << "\nProvider: Step 2. Register method handlers" << std::endl;
    auto register_handler_or_fail = [&method_call_counter](auto& method, std::size_t method_idx) {
        const auto registration_result = method.RegisterHandler(HandlerMaker(method_call_counter, method_idx));
        if (!registration_result.has_value())
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
        PrintLine("Provider: Waiting for Consumer", i, " to finish...\n");
        if (!process_synchronizers.at(i)->WaitWithAbort(stop_token))
        {
            FailTest("Provider: WaitWithAbort for Consumer", i, " was stopped by stop_token\n");
        }
    }

    std::cout << "\nProvider: Step 5. Verify the number of method calls are correct" << std::endl;
    bool test_failed{false};
    for (std::size_t consumer_id{0U}; consumer_id < num_consumers; ++consumer_id)
    {
        const auto& enabled_methods_for_consumer = kEnabledMethodsPerProxy.at(consumer_id);
        for (std::size_t proxy_id{0U}; proxy_id < num_proxies_per_consumer; ++proxy_id)
        {
            for (std::size_t method_id{0U}; method_id < kNumRegisteredMethods; ++method_id)
            {
                const auto is_method_enabled =
                    std::find(enabled_methods_for_consumer.cbegin(), enabled_methods_for_consumer.cend(), method_id) !=
                    enabled_methods_for_consumer.cend();
                if (is_method_enabled)
                {
                    const auto actual_method_call_count = method_call_counter.Get(consumer_id, proxy_id, method_id);

                    const auto expected_method_calls_for_bucket = is_method_enabled ? num_method_calls_per_proxy : 0U;

                    // clang-format off
                    PrintLine("Provider: Consumer process ", consumer_id, ", Proxy ", proxy_id, ", method", method_id, 
                              " -> Expected: ", expected_method_calls_for_bucket, ", Actual: ", actual_method_call_count, '\n');
                    // clang-format on
                    if (expected_method_calls_for_bucket != actual_method_call_count)
                    {
                        test_failed = true;
                    }
                }
            }
        }
    }

    if (test_failed)
    {
        FailTest("Provider: Method call counts did not match expected values");
    }

    std::cout << "Provider: Shutting down.\n";
}

}  // namespace score::mw::com::test
