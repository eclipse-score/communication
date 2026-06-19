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
#include "score/mw/com/test/methods/basic_acceptance_test/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/basic_acceptance_test/test_method_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <unistd.h>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/basic_acc_test_interprocess_notification"};

constexpr std::int32_t kTestValueA = 42;
constexpr std::int32_t kTestValueB = 17;
const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/basic_acceptance_test/TestMethods"}).value();

void CallMethodWithCopy(TestMethodProxy& proxy)
{
    std::cout << "\n=== Test: with_in_args_and_return (copy call) ===" << std::endl;
    auto method_return_result = proxy.with_in_args_and_return(kTestValueA, kTestValueB);
    if (!(method_return_result.has_value()))
    {
        FailTest("Consumer: with_in_args_and_return call failed: ", method_return_result.error());
    }
    const auto actual_return_value = *(method_return_result.value());

    const auto expected_result_value = kTestValueA + kTestValueB;
    if (actual_return_value != expected_result_value)
    {
        FailTest(
            "Consumer: with_in_args_and_return expected ", expected_result_value, " but got ", actual_return_value);
    }

    std::cout << "Consumer: with_in_args_and_return returned correct result: " << actual_return_value << std::endl;
}

void CallMethodZeroCopy(TestMethodProxy& proxy)
{
    std::cout << "\n=== Test: with_in_args_and_return (zero-copy) ===" << std::endl;
    auto allocated_args_result = proxy.with_in_args_and_return.Allocate();
    if (!allocated_args_result.has_value())
    {
        FailTest("Consumer: Could not allocate method args: ", allocated_args_result.error());
    }

    auto& [arg1_ptr, arg2_ptr] = allocated_args_result.value();
    *arg1_ptr = kTestValueA;
    *arg2_ptr = kTestValueB;

    auto method_return_result = proxy.with_in_args_and_return(std::move(arg1_ptr), std::move(arg2_ptr));
    if (!(method_return_result.has_value()))
    {
        FailTest("Consumer: with_in_args_and_return zero-copy call failed: ", method_return_result.error());
    }
    const auto actual_return_value = *(method_return_result.value());

    const auto expected_result_value = kTestValueA + kTestValueB;
    if (actual_return_value != expected_result_value)
    {
        FailTest("Consumer: with_in_args_and_return zero-copy expected ",
                 expected_result_value,
                 " but got ",
                 actual_return_value);
    }

    std::cout << "Consumer: with_in_args_and_return zero-copy returned correct result: " << actual_return_value
              << std::endl;
}

}  // namespace

void run_consumer()
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("Methods basic_acceptance_test consumer failed: Could not create ProcessSynchronizer");
    }

    // Set an exit function to notify the provider in case of failure in calls to FailTest, so that it does not wait
    // indefinitely for the consumer to finish. Will also be called when the guard goes out of scope at the end of this
    // function.
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    ProxyContainer<TestMethodProxy> proxy_container{};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1" << std::endl;
    proxy_container.CreateProxy(kInstanceSpecifier, "basic_acceptance_test");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Call method with copy
    std::cout << "\nConsumer: Step 2" << std::endl;
    CallMethodWithCopy(proxy);

    // Step 3. Call zero-copy method
    std::cout << "\nConsumer: Step 3" << std::endl;
    CallMethodZeroCopy(proxy);
}

}  // namespace score::mw::com::test
