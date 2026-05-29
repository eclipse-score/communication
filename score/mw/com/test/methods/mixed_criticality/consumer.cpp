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
#include "score/mw/com/test/methods/mixed_criticality/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_method_consumer.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <unistd.h>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/mixed_criticality_test_interprocess_notification"};
const std::string kFailureMessagePrefix{"mixed_criticality"};

// Test values for methods with InArgs and Return
constexpr std::int32_t kTestValueA = 42;
constexpr std::int32_t kTestValueB = 23;

// Test values for method with Return only
const std::int32_t kReturnOnlyMethodReturnValue{15};

// Test values for method with InArgs only
const std::int32_t kInArgOnlyMethodTestValueA{17};
const std::int32_t kInArgOnlyMethodTestValueB{18};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/mixed_criticality/MethodSignature"}).value();

}  // namespace

void run_consumer()
{
    AllSignaturesMethodConsumer consumer{};
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("Methods mixed_criticality consumer failed: Could not create ProcessSynchronizer");
        return;
    }

    // Set an exit function to notify the provider in case of failure in calls to FailTest, so that it does not wait
    // indefinitely for the consumer to finish. Will also be called when the guard goes out of scope at the end of
    // this function.
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1" << std::endl;
    consumer.CreateProxy(kInstanceSpecifier, kFailureMessagePrefix);

    // Step 2. Call method with InArgs and Return with copy
    std::cout << "\nConsumer: Step 2" << std::endl;
    consumer.CallMethodWithInArgsAndReturn(
        kTestValueA, kTestValueB, AllSignaturesMethodConsumer::CopyMode::WITH_COPY, kFailureMessagePrefix);

    // Step 3. Call zero-copy method with InArgs and Return
    std::cout << "\nConsumer: Step 3" << std::endl;
    consumer.CallMethodWithInArgsAndReturn(
        kTestValueA, kTestValueB, AllSignaturesMethodConsumer::CopyMode::ZERO_COPY, kFailureMessagePrefix);

    // Step 4. Call method with InArgs only with copy
    std::cout << "\nConsumer: Step 4" << std::endl;
    consumer.CallMethodWithInArgsOnly(kInArgOnlyMethodTestValueA,
                                      kInArgOnlyMethodTestValueB,
                                      AllSignaturesMethodConsumer::CopyMode::WITH_COPY,
                                      kFailureMessagePrefix);

    // Step 5. Call zero-copy method with InArgs only
    std::cout << "\nConsumer: Step 5" << std::endl;
    consumer.CallMethodWithInArgsOnly(kInArgOnlyMethodTestValueA,
                                      kInArgOnlyMethodTestValueB,
                                      AllSignaturesMethodConsumer::CopyMode::ZERO_COPY,
                                      kFailureMessagePrefix);

    // Step 6. Call method with return only with copy
    std::cout << "\nConsumer: Step 6" << std::endl;
    consumer.CallMethodWithReturnOnly(kReturnOnlyMethodReturnValue, kFailureMessagePrefix);

    // Step 7. Call method without InArgs or return
    std::cout << "\nConsumer: Step 7" << std::endl;
    consumer.CallMethodWithoutInArgsOrReturn(kFailureMessagePrefix);
}

}  // namespace score::mw::com::test
