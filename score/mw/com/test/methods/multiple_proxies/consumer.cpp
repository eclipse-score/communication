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
#include "score/mw/com/test/methods/multiple_proxies/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/methods_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"
#include "score/mw/com/test/methods/multiple_proxies/duplicate_signatures_datatype.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <unistd.h>
#include <cstddef>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/multiple_proxies_test_interprocess_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"multiple_proxies/MultiMethodProvider"}).value();

enum class CopyMode
{
    ZERO_COPY,
    WITH_COPY
};

void ValidateMethodCallResult(score::Result<MethodReturnTypePtr<DuplicateSignatureProxy::ReturnType>>& result,
                              std::size_t consumer_id,
                              std::size_t method_id,
                              std::int32_t input_value)
{
    if (!result.has_value())
    {
        FailTest("Consumer", consumer_id, ": method", method_id, " call failed: ", result.error());
    }

    const auto actual = *(result.value());
    const auto expected = input_value * (kMethodResultMultiplierBase + static_cast<std::int32_t>(method_id));

    // clang-format off
    if (actual != expected)
    {
        FailTest("Consumer" , consumer_id , ": method" , method_id , " returned " , actual , " but expected " , expected);
    }
    // clang-format on

    std::cout << "Consumer" << consumer_id << ": method" << method_id << " returned correct value: " << actual << '\n';
}

void CallMethod(DuplicateSignatureProxy& proxy,
                CopyMode copy_mode,
                std::size_t method_id,
                std::size_t consumer_id,
                std::int32_t input)
{
    std::cout << "Consumer" << consumer_id << ": Calling a method" << method_id << " with the value " << input << '\n';

    auto& method = [&proxy, consumer_id, method_id]() -> auto& {
        if (method_id == 0U)
        {
            return proxy.method0;
        }
        if (method_id == 1U)
        {
            return proxy.method1;
        }
        if (method_id == 2U)
        {
            return proxy.method2;
        }
        if (method_id == 3U)
        {
            return proxy.method3;
        }
        if (method_id == 4U)
        {
            return proxy.method4;
        }
        // clang-format off
        FailTest("Consumer", consumer_id, ": Invalid method ID ", method_id, " cannot be larger than ", kNumRegisteredMethods);
        // clang-format on

        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }();

    auto result =
        [&method, copy_mode, input]() -> score::Result<MethodReturnTypePtr<DuplicateSignatureProxy::ReturnType>> {
        if (copy_mode == CopyMode::ZERO_COPY)
        {
            auto allocated_args_result = method.Allocate();
            if (!allocated_args_result.has_value())
            {
                std::cerr << "Consumer: Failure during zero-copy call. " << allocated_args_result.error() << '\n';
                return score::Unexpected(allocated_args_result.error());
            }

            auto& [arg_ptr] = allocated_args_result.value();
            *arg_ptr = input;

            return method(std::move(arg_ptr));
        }

        return method(input);
    }();

    ValidateMethodCallResult(result, consumer_id, method_id, input);
}

}  // namespace

void run_consumer(const std::size_t consumer_id,
                  const std::size_t num_proxies_per_process,
                  const std::size_t num_method_calls_per_proxy,
                  const std::vector<std::size_t>& enabled_method_ids)
{
    const auto notification_path = CreateInterprocessNotificationShmPath(consumer_id);

    auto process_synchronizer_result = ProcessSynchronizer::Create(notification_path);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("Methods signature_variations consumer failed: Could not create ProcessSynchronizer");
    }

    // Set an exit function to notify the provider in case of failure in calls to FailTest, so that it does not wait
    // indefinitely for the consumer to finish. Will also be called when the guard goes out of scope at the end of
    // this function.
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    auto proxy_runner = [&enabled_method_ids, consumer_id, num_method_calls_per_proxy](std::size_t proxy_id) -> void {
        ProxyContainer<DuplicateSignatureProxy> proxy_container{};

        // Step 1. Find service and create proxy
        std::cout << "\nConsumer: Step 1 - Proxy " << proxy_id << std::endl;
        proxy_container.CreateProxy(kInstanceSpecifier, "multiple_proxies");
        auto& proxy = proxy_container.GetProxy();

        // Step 2. Call method with InArgs and Return with copy
        std::cout << "\nConsumer: Step 2 - Proxy " << proxy_id << std::endl;
        for (std::size_t i = 0U; i < num_method_calls_per_proxy; ++i)
        {
            for (const auto& method_id : enabled_method_ids)
            {
                const auto some_random_input_value = static_cast<std::int32_t>(proxy_id + method_id);
                CopyMode copy_mode = (method_id % 2 == 0) ? CopyMode::ZERO_COPY : CopyMode::WITH_COPY;
                CallMethod(proxy, copy_mode, method_id, consumer_id, some_random_input_value);
            }
        }
    };

    {
        std::vector<score::cpp::jthread> threads{};
        threads.reserve(num_proxies_per_process);
        for (std::size_t proxy_id = 0U; proxy_id < num_proxies_per_process; ++proxy_id)
        {
            threads.emplace_back(proxy_runner, proxy_id);
        }
    }

    std::cout << "Consumer" << consumer_id << ": All threads have completed. Exiting." << std::endl;
}

}  // namespace score::mw::com::test
