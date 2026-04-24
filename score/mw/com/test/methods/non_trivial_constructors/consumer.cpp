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
#include "score/mw/com/test/methods/non_trivial_constructors/consumer.h"

#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/methods_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/non_trivial_constructors/test_method_datatype.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <unistd.h>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/non_trivial_constructors_test_interprocess_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/non_trivial_constructors/MethodSignature"}).value();

bool CallMethodWithInArgsAndReturn(NonTrivialConstructorProxy& proxy)
{
    auto call_result = [&proxy]() -> score::Result<impl::MethodReturnTypePtr<NonTriviallyConstructibleType>> {
        std::cout << "\n=== Test: with_in_args_and_return (zero-copy) ===" << std::endl;
        auto allocated_args_result = proxy.with_in_args_and_return.Allocate();
        if (!allocated_args_result.has_value())
        {
            std::cerr << "Consumer: Could not allocate method args: " << allocated_args_result.error() << std::endl;
            return Unexpected(allocated_args_result.error());
        }

        auto& [arg1_ptr, arg2_ptr] = allocated_args_result.value();
        return proxy.with_in_args_and_return(std::move(arg1_ptr), std::move(arg2_ptr));
    }();
    if (!call_result.has_value())
    {
        std::cerr << "Consumer: with_in_args_and_return call failed: " << call_result.error() << std::endl;
        return false;
    }
    const auto return_value = *(call_result.value());

    // Since provider adds the two input args which are both initialized with kInitialValue
    const auto expected_return_value = NonTriviallyConstructibleType{} + NonTriviallyConstructibleType{};
    if (return_value != expected_return_value)
    {
        std::cerr << "Consumer: Expected " << expected_return_value << " but got " << return_value << std::endl;
        return false;
    }

    std::cout << "Consumer: with_in_args_and_return returned correct result: " << return_value << std::endl;
    return true;
}

bool CallMethodWithInArgsOnly(NonTrivialConstructorProxy& proxy)
{
    auto call_result = [&proxy]() -> Result<void> {
        std::cout << "\n=== Test: with_in_args_only (zero-copy) ===" << std::endl;
        auto allocated_args_result = proxy.with_in_args_only.Allocate();
        if (!allocated_args_result.has_value())
        {
            std::cerr << "Consumer: Could not allocate method args: " << allocated_args_result.error() << std::endl;
            return Unexpected(allocated_args_result.error());
        }

        auto& [arg1_ptr, arg2_ptr] = allocated_args_result.value();

        return proxy.with_in_args_only(std::move(arg1_ptr), std::move(arg2_ptr));
    }();
    if (!call_result.has_value())
    {
        std::cerr << "Consumer: with_in_args_only call failed: " << call_result.error() << std::endl;
        return false;
    }

    std::cout << "Consumer: with_in_args_only returned without error" << std::endl;
    return true;
}

bool CallMethodWithReturnOnly(NonTrivialConstructorProxy& proxy)
{
    std::cout << "\n=== Test: with_return_only (copy call) ===" << std::endl;
    const auto call_result = proxy.with_return_only();
    if (!call_result.has_value())
    {
        std::cerr << "Consumer: with_return_only call failed: " << call_result.error() << std::endl;
        return false;
    }
    const auto return_value = *(call_result.value());

    const auto expected_return_value = NonTriviallyConstructibleType{};
    if (return_value != expected_return_value)
    {
        std::cerr << "Consumer: Expected " << expected_return_value << " but got " << return_value << std::endl;
        return false;
    }

    std::cout << "Consumer: with_return_only returned correct result: " << return_value << std::endl;
    return true;
}

}  // namespace

int run_consumer()
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        std::cerr << "Methods non_trivial_constructors consumer failed: Could not create ProcessSynchronizer"
                  << std::endl;
        return EXIT_FAILURE;
    }

    ProxyContainer<NonTrivialConstructorProxy> consumer{};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1" << std::endl;
    if (!consumer.CreateProxy(kInstanceSpecifier, {"with_in_args_and_return", "with_in_args_only", "with_return_only"}))
    {
        std::cerr << "Methods non_trivial_constructors consumer failed: CreateProxy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 2. Call zero-copy method with InArgs and Return
    std::cout << "\nConsumer: Step 2" << std::endl;
    if (!CallMethodWithInArgsAndReturn(consumer.GetProxy()))
    {
        std::cerr << "Methods non_trivial_constructors consumer failed: CallMethodWithInArgsAndReturnZeroCopy"
                  << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 3. Call zero-copy method with InArgs only
    std::cout << "\nConsumer: Step 3" << std::endl;
    if (!CallMethodWithInArgsOnly(consumer.GetProxy()))
    {
        std::cerr << "Methods non_trivial_constructors consumer failed: CallMethodWithInArgsOnlyZeroCopy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 4. Call method with return only with copy
    std::cout << "\nConsumer: Step 4" << std::endl;
    if (!CallMethodWithReturnOnly(consumer.GetProxy()))
    {
        std::cerr << "Methods non_trivial_constructors consumer failed: CallMethodWithReturnOnly" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    process_synchronizer_result->Notify();

    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
