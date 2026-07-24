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
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_method_consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include <chrono>
#include <thread>

namespace score::mw::com::test
{

void AllSignaturesMethodConsumer::CreateProxy(InstanceSpecifier instance_specifier,
                                              const std::string& failure_message_prefix)
{
    proxy_container_.CreateProxy(std::move(instance_specifier), failure_message_prefix);
}

AllSignaturesProxy& AllSignaturesMethodConsumer::GetProxy()
{
    return proxy_container_.GetProxy();
}

void AllSignaturesMethodConsumer::CallMethodWithInArgsAndReturn(const std::int32_t input_argument_a,
                                                                const std::int32_t input_argument_b,
                                                                const CopyMode copy_mode,
                                                                const std::string& failure_message_prefix)
{
    auto& proxy = GetProxy();

    auto call_result = [&proxy,
                        copy_mode,
                        input_argument_a,
                        input_argument_b]() -> score::Result<impl::MethodReturnTypePtr<std::int32_t>> {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(copy_mode == CopyMode::WITH_COPY || copy_mode == CopyMode::ZERO_COPY);
        if (copy_mode == CopyMode::ZERO_COPY)
        {
            std::cout << "\n=== Test: with_in_args_and_return (zero-copy) ===" << std::endl;
            auto allocated_args_result = proxy.with_in_args_and_return.Allocate();
            if (!allocated_args_result.has_value())
            {
                std::cout << "Consumer: Could not allocate method args: " << allocated_args_result.error() << std::endl;
                return Unexpected(allocated_args_result.error());
            }

            auto& [arg1_ptr, arg2_ptr] = allocated_args_result.value();
            *arg1_ptr = input_argument_a;
            *arg2_ptr = input_argument_b;

            return proxy.with_in_args_and_return(std::move(arg1_ptr), std::move(arg2_ptr));
        }
        else
        {
            std::cout << "\n=== Test: with_in_args_and_return (copy call) ===" << std::endl;
            return proxy.with_in_args_and_return(input_argument_a, input_argument_b);
        }
    }();
    if (!call_result.has_value())
    {
        std::this_thread::sleep_for(std::chrono::seconds{2});
        FailTest(failure_message_prefix, " Consumer: with_in_args_and_return call failed: ", call_result.error());
    }
    const auto return_value = *(call_result.value());

    const auto expected_result_value = input_argument_a + input_argument_b;
    if (return_value != expected_result_value)
    {
        FailTest(failure_message_prefix, " Consumer: Expected ", expected_result_value, " but got ", return_value);
    }

    std::cout << "Consumer: with_in_args_and_return returned correct result: " << return_value << std::endl;
}

void AllSignaturesMethodConsumer::CallMethodWithInArgsOnly(const std::int32_t input_argument_a,
                                                           const std::int32_t input_argument_b,
                                                           const CopyMode copy_mode,
                                                           const std::string& failure_message_prefix)
{
    auto& proxy = GetProxy();

    auto call_result = [&proxy, copy_mode, input_argument_a, input_argument_b]() -> Result<void> {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(copy_mode == CopyMode::WITH_COPY || copy_mode == CopyMode::ZERO_COPY);
        if (copy_mode == CopyMode::ZERO_COPY)
        {
            std::cout << "\n=== Test: with_in_args_only (zero-copy) ===" << std::endl;
            auto allocated_args_result = proxy.with_in_args_only.Allocate();
            if (!allocated_args_result.has_value())
            {
                std::cerr << "Consumer: Could not allocate method args: " << allocated_args_result.error() << std::endl;
                return Unexpected(allocated_args_result.error());
            }

            auto& [arg1_ptr, arg2_ptr] = allocated_args_result.value();
            *arg1_ptr = input_argument_a;
            *arg2_ptr = input_argument_b;

            return proxy.with_in_args_only(std::move(arg1_ptr), std::move(arg2_ptr));
        }
        else
        {
            std::cout << "\n=== Test: with_in_args_only (copy call) ===" << std::endl;
            return proxy.with_in_args_only(input_argument_a, input_argument_b);
        }
    }();
    if (!call_result.has_value())
    {
        FailTest(failure_message_prefix, " Consumer: with_in_args_only call failed: ", call_result.error());
    }

    std::cout << "Consumer: with_in_args_only returned without error" << std::endl;
}

void AllSignaturesMethodConsumer::CallMethodWithReturnOnly(const std::int32_t expected_return_value,
                                                           const std::string& failure_message_prefix)
{
    auto& proxy = GetProxy();

    std::cout << "\n=== Test: with_return_only (copy call) ===" << std::endl;
    const auto call_result = proxy.with_return_only();
    if (!call_result.has_value())
    {
        FailTest(failure_message_prefix, " Consumer: with_return_only call failed: ", call_result.error());
    }
    const auto return_value = *(call_result.value());

    if (return_value != expected_return_value)
    {
        FailTest(failure_message_prefix, " Consumer: Expected ", expected_return_value, " but got ", return_value);
    }

    std::cout << "Consumer: with_return_only returned correct result: " << return_value << std::endl;
}

void AllSignaturesMethodConsumer::CallMethodWithoutInArgsOrReturn(const std::string& failure_message_prefix)
{
    auto& proxy = GetProxy();

    std::cout << "\n=== Test: without_args_or_return (copy call) ===" << std::endl;
    const auto return_type_ptr_result = proxy.without_args_or_return();
    if (!return_type_ptr_result.has_value())
    {
        FailTest(
            failure_message_prefix, " Consumer: without_args_or_return call failed: ", return_type_ptr_result.error());
    }

    std::cout << "Consumer: without_args_or_return returned without error" << std::endl;
}

}  // namespace score::mw::com::test
