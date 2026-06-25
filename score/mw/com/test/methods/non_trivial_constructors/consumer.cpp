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

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
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

void CallMethodWithInArgsAndReturn(NonTrivialConstructorProxy& proxy, const std::string& failure_message_prefix)
{
    auto call_result = [&proxy]() -> score::Result<impl::MethodReturnTypePtr<NonTriviallyConstructibleType>> {
        std::cout << "\n=== Test: with_in_args_and_return (zero-copy) ===" << std::endl;
        auto allocated_args_result = proxy.with_in_args_and_return.Allocate();
        if (!allocated_args_result.has_value())
        {
            return Unexpected(allocated_args_result.error());
        }

        auto& [arg1_ptr, arg2_ptr] = allocated_args_result.value();
        return proxy.with_in_args_and_return(std::move(arg1_ptr), std::move(arg2_ptr));
    }();
    if (!call_result.has_value())
    {
        FailTest(failure_message_prefix, " Consumer: with_in_args_and_return call failed: ", call_result.error());
    }
    const auto return_value = *(call_result.value());

    // Since provider adds the two input args which are both initialized with kInitialValue
    const auto expected_return_value = NonTriviallyConstructibleType{} + NonTriviallyConstructibleType{};
    if (return_value != expected_return_value)
    {
        FailTest(failure_message_prefix, " Consumer: Expected ", expected_return_value, " but got ", return_value);
    }

    std::cout << "Consumer: with_in_args_and_return returned correct result: " << return_value << std::endl;
}

void CallMethodWithInArgsOnly(NonTrivialConstructorProxy& proxy, const std::string& failure_message_prefix)
{
    auto call_result = [&proxy]() -> Result<void> {
        std::cout << "\n=== Test: with_in_args_only (zero-copy) ===" << std::endl;
        auto allocated_args_result = proxy.with_in_args_only.Allocate();
        if (!allocated_args_result.has_value())
        {
            return Unexpected(allocated_args_result.error());
        }

        auto& [arg1_ptr, arg2_ptr] = allocated_args_result.value();

        return proxy.with_in_args_only(std::move(arg1_ptr), std::move(arg2_ptr));
    }();
    if (!call_result.has_value())
    {
        FailTest(failure_message_prefix, " Consumer: with_in_args_only call failed: ", call_result.error());
    }

    std::cout << "Consumer: with_in_args_only returned without error" << std::endl;
}

void CallMethodWithReturnOnly(NonTrivialConstructorProxy& proxy, const std::string& failure_message_prefix)
{
    std::cout << "\n=== Test: with_return_only (copy call) ===" << std::endl;
    const auto call_result = proxy.with_return_only();
    if (!call_result.has_value())
    {
        FailTest(failure_message_prefix, " Consumer: with_return_only call failed: ", call_result.error());
    }
    const auto return_value = *(call_result.value());

    const auto expected_return_value = NonTriviallyConstructibleType{};
    if (return_value != expected_return_value)
    {
        FailTest(failure_message_prefix, " Consumer: Expected ", expected_return_value, " but got ", return_value);
    }

    std::cout << "Consumer: with_return_only returned correct result: " << return_value << std::endl;
}

}  // namespace

void run_consumer()
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("Methods non_trivial_constructors consumer failed: Could not create ProcessSynchronizer");
    }

    // Set an exit function to notify the provider in case of failure in calls to FailTest, so that it does not wait
    // indefinitely for the consumer to finish. Will also be called when the guard goes out of scope at the end of this
    // function.
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    ProxyContainer<NonTrivialConstructorProxy> consumer{};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1" << std::endl;
    consumer.CreateProxy(kInstanceSpecifier, "non_trivial_constructors");

    // Step 2. Call zero-copy method with InArgs and Return
    std::cout << "\nConsumer: Step 2" << std::endl;
    CallMethodWithInArgsAndReturn(consumer.GetProxy(), "non_trivial_constructors");

    // Step 3. Call zero-copy method with InArgs only
    std::cout << "\nConsumer: Step 3" << std::endl;
    CallMethodWithInArgsOnly(consumer.GetProxy(), "non_trivial_constructors");

    // Step 4. Call method with return only with copy
    std::cout << "\nConsumer: Step 4" << std::endl;
    CallMethodWithReturnOnly(consumer.GetProxy(), "non_trivial_constructors");
}

}  // namespace score::mw::com::test
