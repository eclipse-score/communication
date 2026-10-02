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
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"
#include "score/mw/com/test/methods/multiple_proxies/duplicate_signatures_datatype.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <unistd.h>
#include <cstddef>
#include <cstdlib>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/multiple_proxies_test_interprocess_notification"};

enum class CopyMode
{
    ZERO_COPY,
    WITH_COPY
};

void ValidateMethodCallResult(
    score::Result<MethodReturnTypePtr<DuplicateSignatureProxy::ReturnType>>& method_call_return_result,
    std::size_t proxy_id,
    std::size_t consumer_id,
    std::size_t method_id,
    std::size_t call_idx,
    std::int32_t input_value)
{
    if (!method_call_return_result.has_value())
    {
        // clang-format off
        FailTest("Consumer process ", consumer_id, ", Proxy ", proxy_id, ": method", method_id, " call failed: ", method_call_return_result.error());
        // clang-format on
    }

    const auto method_call_return = *(method_call_return_result.value());
    const bool is_method_return_valid =
        ((method_call_return.sent_value == input_value) && (method_call_return.call_index == call_idx) &&
         (method_call_return.consumer_id == consumer_id) && (method_call_return.proxy_id == proxy_id));

    if (!is_method_return_valid)
    {
        // clang-format off
        FailTest("Consumer process ", consumer_id, ", Proxy ", proxy_id, ": method", method_id,
                 " returned unexpected payload. Actual={sent_value:", method_call_return.sent_value,
                 ", call_count:", method_call_return.call_index,
                 ", consumer_id:", method_call_return.consumer_id,
                 ", proxy_id:", method_call_return.proxy_id,
                 "} Expected={sent_value:", input_value,
                 ", call_count:", call_idx,
                 ", consumer_id:", consumer_id,
                 ", proxy_id:", proxy_id,
                 "}");
        // clang-format on
    }
}

void CallMethod(DuplicateSignatureProxy& proxy,
                CopyMode copy_mode,
                std::size_t proxy_id,
                std::size_t method_id,
                std::size_t consumer_id,
                std::size_t call_idx,
                std::int32_t input_value)
{
    // clang-format off
    PrintLine("Consumer process ", consumer_id, ", Proxy ", proxy_id, ": Calling a method", method_id,
              " with the value ", call_idx, ". Call count = ", call_idx, '\n');
    // clang-format on

    auto& method = [&proxy, proxy_id, consumer_id, method_id]() -> auto& {
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
        FailTest("Consumer process ", consumer_id, ", Proxy ", proxy_id, ": Invalid method ID ", method_id, " cannot be larger than ", kNumRegisteredMethods);
        // clang-format on

        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }();

    auto method_call_result = [&method, copy_mode, call_idx, consumer_id, proxy_id, input_value]()
        -> score::Result<MethodReturnTypePtr<DuplicateSignatureProxy::ReturnType>> {
        if (copy_mode == CopyMode::ZERO_COPY)
        {
            auto allocated_args_result = method.Allocate();
            if (!allocated_args_result.has_value())
            {
                // clang-format off
                FailTest("Consumer process ", consumer_id, ", Proxy ", proxy_id, ": Failure during zero-copy call. ", allocated_args_result.error(), '\n');
                // clang-format on
            }

            auto& [arg_ptr, call_index_ptr, consumer_id_ptr, proxy_id_ptr] = allocated_args_result.value();
            *arg_ptr = input_value;
            *call_index_ptr = call_idx;
            *consumer_id_ptr = consumer_id;
            *proxy_id_ptr = proxy_id;

            return method(
                std::move(arg_ptr), std::move(call_index_ptr), std::move(consumer_id_ptr), std::move(proxy_id_ptr));
        }

        return method(input_value, call_idx, consumer_id, proxy_id);
    }();

    ValidateMethodCallResult(method_call_result, proxy_id, consumer_id, method_id, call_idx, input_value);
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
        PrintLine("Consumer process ", consumer_id, ", Proxy ", proxy_id, ": Step 1 - Create proxy\n");
        const auto instance_specifier =
            InstanceSpecifier::Create(std::string{"multiple_proxies/MultiMethodConsumer"} + std::to_string(consumer_id))
                .value();
        proxy_container.CreateProxy(instance_specifier, "multiple_proxies");
        auto& proxy = proxy_container.GetProxy();

        // Step 2. Call method with InArgs and Return with copy
        PrintLine("Consumer process ", consumer_id, ", Proxy ", proxy_id, ": Step 2 - Call methods\n");
        std::array<std::size_t, kNumRegisteredMethods> num_method_calls_per_method{};
        num_method_calls_per_method.fill(0U);
        for (std::size_t i = 0U; i < num_method_calls_per_proxy; ++i)
        {
            for (const auto& method_id : enabled_method_ids)
            {
                const auto some_random_input_value = static_cast<std::int32_t>(proxy_id + method_id);
                CopyMode copy_mode = (method_id % 2 == 0) ? CopyMode::ZERO_COPY : CopyMode::WITH_COPY;

                num_method_calls_per_method.at(method_id)++;
                CallMethod(proxy,
                           copy_mode,
                           proxy_id,
                           method_id,
                           consumer_id,
                           num_method_calls_per_method.at(method_id),
                           some_random_input_value);
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

    PrintLine("Consumer process ", consumer_id, ": All threads have completed. Exiting.\n");
}

}  // namespace score::mw::com::test
