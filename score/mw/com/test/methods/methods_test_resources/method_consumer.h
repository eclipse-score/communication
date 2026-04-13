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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_METHOD_CONSUMER_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_METHOD_CONSUMER_H

#include "score/mw/com/test/methods/methods_test_resources/proxy_container.h"

#include <cstdint>

namespace score::mw::com::test
{

/// \brief Helper class which provides consumer side method functionality for testing
///
/// This class can be used in conjunction with MethodProvider for abstracting method-related boilerplate code out of
/// tests. The Proxy type used to template this class should only contain (zero or more) methods which follow the
/// following structure:
///
///   - Method with InArg and Return
///     - Method variable name: with_in_args_and_return
///     - Signature: `std::int32_t(std::int32_t, std::int32_t)`
///     - Behaviour: returns the sum of the arguments.
///     - Verifications: Consumer verifies that the returned value is the sum of the inputs
///
///   - Method with InArgs only
///     - Method variable name: with_in_args_only
///     - Signature: `void(std::int32_t, std::int32_t)`
///     - Behaviour: Does nothing with provided arguments
///     - Verifications: Provider verifies that the provided args are expected values (expected values provided by
///     individual test)
///
///   - Method with Return only
///     - Method variable name: with_return_only
///     - Signature: `std::int32_t()`
///     - Behaviour: Does nothing
///     - Verifications: Consumer verifies that the return value is as expected (expected value provided by individual
///     test)
///
///   - Method without InArgs / Return
///     - Method variable name: without_args_or_return
///     - Signature: `void()`
///     - Behaviour: Does nothing
///     - Verifications: No verification done.
template <typename Proxy>
class MethodConsumer
{
  public:
    enum class CopyMode
    {
        ZERO_COPY,
        WITH_COPY
    };

    bool CreateProxy(InstanceSpecifier instance_specifier, const std::vector<std::string_view>& enabled_method_names);

    bool CallMethodWithInArgsAndReturn(const std::int32_t input_argument_a,
                                       const std::int32_t input_argument_b,
                                       const CopyMode copy_mode);

    bool CallMethodWithInArgsOnly(const std::int32_t input_argument_a,
                                  const std::int32_t input_argument_b,
                                  const CopyMode copy_mode);

    bool CallMethodWithReturnOnly(const std::int32_t expected_return_value);

    bool CallMethodWithoutInArgsOrReturn();

  private:
    ProxyContainer<Proxy> proxy_container_;
};

template <typename Proxy>
bool MethodConsumer<Proxy>::CreateProxy(InstanceSpecifier instance_specifier,
                                        const std::vector<std::string_view>& enabled_method_names)
{
    return proxy_container_.CreateProxy(std::move(instance_specifier), enabled_method_names);
}

template <typename Proxy>
bool MethodConsumer<Proxy>::CallMethodWithInArgsAndReturn(const std::int32_t input_argument_a,
                                                          const std::int32_t input_argument_b,
                                                          const CopyMode copy_mode)
{
    auto& proxy = proxy_container_.GetProxy();

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
                std::cerr << "Consumer: Could not allocate method args: " << allocated_args_result.error() << std::endl;
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
        std::cerr << "Consumer: with_in_args_and_return call failed: " << call_result.error() << std::endl;
        return false;
    }
    const auto return_value = *(call_result.value());

    const auto expected_result_value = input_argument_a + input_argument_b;
    if (return_value != expected_result_value)
    {
        std::cerr << "Consumer: Expected " << expected_result_value << " but got " << return_value << std::endl;
        return false;
    }

    std::cout << "Consumer: with_in_args_and_return returned correct result: " << return_value << std::endl;
    return true;
}

template <typename Proxy>
bool MethodConsumer<Proxy>::CallMethodWithInArgsOnly(const std::int32_t input_argument_a,
                                                     const std::int32_t input_argument_b,
                                                     const CopyMode copy_mode)
{
    auto& proxy = proxy_container_.GetProxy();

    auto call_result = [&proxy, copy_mode, input_argument_a, input_argument_b]() -> ResultBlank {
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
        std::cerr << "Consumer: with_in_args_only call failed: " << call_result.error() << std::endl;
        return false;
    }

    std::cout << "Consumer: with_in_args_only returned without error" << std::endl;
    return true;
}

template <typename Proxy>
bool MethodConsumer<Proxy>::CallMethodWithReturnOnly(const std::int32_t expected_return_value)
{
    auto& proxy = proxy_container_.GetProxy();

    std::cout << "\n=== Test: with_return_only (copy call) ===" << std::endl;
    const auto call_result = proxy.with_return_only();
    if (!call_result.has_value())
    {
        std::cerr << "Consumer: with_return_only call failed: " << call_result.error() << std::endl;
        return false;
    }
    const auto return_value = *(call_result.value());

    if (return_value != expected_return_value)
    {
        std::cerr << "Consumer: Expected " << expected_return_value << " but got " << return_value << std::endl;
        return false;
    }

    std::cout << "Consumer: with_return_only returned correct result: " << return_value << std::endl;
    return true;
}

template <typename Proxy>
bool MethodConsumer<Proxy>::CallMethodWithoutInArgsOrReturn()
{
    auto& proxy = proxy_container_.GetProxy();

    std::cout << "\n=== Test: without_args_or_return (copy call) ===" << std::endl;
    const auto return_type_ptr_result = proxy.without_args_or_return();
    if (!return_type_ptr_result.has_value())
    {
        std::cerr << "Consumer: without_args_or_return call failed: " << return_type_ptr_result.error() << std::endl;
        return false;
    }

    std::cout << "Consumer: without_args_or_return returned without error" << std::endl;
    return true;
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_METHOD_CONSUMER_H
