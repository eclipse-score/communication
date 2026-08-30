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
#include "score/mw/com/test/move_semantics/proxy_method/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/move_semantics/proxy_method/test_method_datatype.h"

#include <cstdint>
#include <iostream>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/proxy_method_move_semantics_interprocess_notification"};

/// \brief Calls the method via the copy path and verifies the returned value.
void CallMethodWithCopy(ProxyMethodMoveSemanticsProxy& proxy, const std::int32_t expected_return_value)
{
    std::cout << "\nConsumer: Calling method (copy), expecting " << expected_return_value << std::endl;
    auto method_return_result = proxy.with_in_args_and_return(kTestValueA, kTestValueB);
    if (!method_return_result.has_value())
    {
        FailTest("Consumer: with_in_args_and_return copy call failed: ", method_return_result.error());
    }
    const auto actual_return_value = *(method_return_result.value());
    if (actual_return_value != expected_return_value)
    {
        FailTest("Consumer: with_in_args_and_return copy call expected ",
                 expected_return_value,
                 " but got ",
                 actual_return_value);
    }
}

/// \brief Calls the method via the zero-copy path and verifies the returned value.
void CallMethodZeroCopy(ProxyMethodMoveSemanticsProxy& proxy, const std::int32_t expected_return_value)
{
    std::cout << "\nConsumer: Calling method (zero-copy), expecting " << expected_return_value << std::endl;
    auto allocated_args_result = proxy.with_in_args_and_return.Allocate();
    if (!allocated_args_result.has_value())
    {
        FailTest("Consumer: Could not allocate method args: ", allocated_args_result.error());
    }

    auto& [arg1_ptr, arg2_ptr] = allocated_args_result.value();
    *arg1_ptr = kTestValueA;
    *arg2_ptr = kTestValueB;

    auto method_return_result = proxy.with_in_args_and_return(std::move(arg1_ptr), std::move(arg2_ptr));
    if (!method_return_result.has_value())
    {
        FailTest("Consumer: with_in_args_and_return zero-copy call failed: ", method_return_result.error());
    }
    const auto actual_return_value = *(method_return_result.value());
    if (actual_return_value != expected_return_value)
    {
        FailTest("Consumer: with_in_args_and_return zero-copy call expected ",
                 expected_return_value,
                 " but got ",
                 actual_return_value);
    }
}

/// \brief Exercises both the copy and zero-copy call paths and verifies the returned value.
void CallAndVerify(ProxyMethodMoveSemanticsProxy& proxy, const std::int32_t expected_return_value)
{
    CallMethodWithCopy(proxy, expected_return_value);
    CallMethodZeroCopy(proxy, expected_return_value);
}

void RunConsumerMoveConstruct(const std::string& failure_message_prefix)
{
    // Step 1. Find service and create proxy A
    std::cout << "\nConsumer: Step 1 - Find service and create proxy A" << std::endl;
    ProxyContainer<ProxyMethodMoveSemanticsProxy> proxy_a_container{};
    proxy_a_container.CreateProxy(kInstanceSpecifierMovedTo, failure_message_prefix);
    auto proxy_a = proxy_a_container.Extract();

    // Step 2. Call method via proxy A (iteration 0: proxy works before the move)
    std::cout << "\nConsumer: Step 2 - Call method via proxy A" << std::endl;
    CallAndVerify(proxy_a, kTestValueA + kTestValueB);

    // Step 3. Move construct proxy B from proxy A
    std::cout << "\nConsumer: Step 3 - Move construct proxy B from proxy A" << std::endl;
    auto proxy_b = std::move(proxy_a);

    // Step 4. Call method via proxy B (iteration 1: proxy works after the move)
    std::cout << "\nConsumer: Step 4 - Call method via proxy B" << std::endl;
    CallAndVerify(proxy_b, kTestValueA + kTestValueB);
}

void RunConsumerMoveAssign(const std::string& failure_message_prefix)
{
    // Step 1. Find service and create proxy A (connected to the moved-from instance, which answers with a + b)
    std::cout << "\nConsumer: Step 1 - Find service and create proxy A" << std::endl;
    ProxyContainer<ProxyMethodMoveSemanticsProxy> proxy_a_container{};
    proxy_a_container.CreateProxy(kInstanceSpecifierMovedFrom, failure_message_prefix);
    auto proxy_a = proxy_a_container.Extract();

    // Step 2. Find service and create proxy B (connected to the moved-to instance, which answers with a - b)
    std::cout << "\nConsumer: Step 2 - Find service and create proxy B" << std::endl;
    ProxyContainer<ProxyMethodMoveSemanticsProxy> proxy_b_container{};
    proxy_b_container.CreateProxy(kInstanceSpecifierMovedTo, failure_message_prefix);
    auto proxy_b = proxy_b_container.Extract();

    // Step 3. Call method via proxy A (iteration 0: proxy A works before the move, returns a + b)
    std::cout << "\nConsumer: Step 3 - Call method via proxy A" << std::endl;
    CallAndVerify(proxy_a, kTestValueA + kTestValueB);

    // Step 4. Call method via proxy B (iteration 0: proxy B works before the move, returns a - b)
    std::cout << "\nConsumer: Step 4 - Call method via proxy B" << std::endl;
    CallAndVerify(proxy_b, kTestValueA - kTestValueB);

    // Step 5. Move assign proxy A into proxy B. Proxy B now holds the moved-from proxy.
    std::cout << "\nConsumer: Step 5 - Move assign proxy A into proxy B" << std::endl;
    proxy_b = std::move(proxy_a);

    // Step 6. Call method via proxy B (iteration 1: proxy B now reaches the moved-from method handler, returns a + b)
    std::cout << "\nConsumer: Step 6 - Call method via proxy B" << std::endl;
    CallAndVerify(proxy_b, kTestValueA + kTestValueB);
}

}  // namespace

void RunConsumer(const ProxyMoveScenario& scenario, const score::cpp::stop_token& /*stop_token*/)
{
    const std::string failure_message_prefix{"proxy_method_move_semantics"};

    auto consumer_done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!consumer_done_synchronizer_result.has_value())
    {
        FailTest("proxy_method_move_semantics consumer failed: could not create consumer done synchronizer");
    }

    // Notify the provider when the consumer is done (or fails) so that it does not wait indefinitely.
    ExitFunctionGuard done_guard{[&consumer_done_synchronizer_result]() {
        consumer_done_synchronizer_result->Notify();
    }};

    switch (scenario)
    {
        case ProxyMoveScenario::kMoveConstructAfterMethodCall:
        {
            RunConsumerMoveConstruct(failure_message_prefix);
            break;
        }
        case ProxyMoveScenario::kMoveAssignAfterMethodCall:
        {
            RunConsumerMoveAssign(failure_message_prefix);
            break;
        }
        case ProxyMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
            FailTest("proxy_method_move_semantics consumer failed: unknown scenario");
    }

    std::cout << "Consumer: Done with all method calls, exiting" << std::endl;
}

}  // namespace score::mw::com::test
