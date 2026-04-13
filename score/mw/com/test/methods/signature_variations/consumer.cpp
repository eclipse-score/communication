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
#include "score/mw/com/test/methods/signature_variations/consumer.h"

#include "score/mw/com/test/methods/methods_test_resources/method_consumer.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/signature_variations/test_method_datatype.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <unistd.h>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/signature_variations_test_interprocess_notification"};

// Test values for methods with InArgs and Return
constexpr std::int32_t kTestValueA = 42;
constexpr std::int32_t kTestValueB = 23;

// Test values for method with Return only
const std::int32_t kReturnOnlyMethodReturnValue{15};

// Test values for method with InArgs only
const std::int32_t kInArgOnlyMethodTestValueA{17};
const std::int32_t kInArgOnlyMethodTestValueB{18};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/signature_variations/MethodSignature"}).value();

}  // namespace

int run_consumer()
{
    MethodConsumer<MethodSignatureProxy> consumer{};
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        std::cerr << "Methods signature_variations consumer failed: Could not create ProcessSynchronizer" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1" << std::endl;
    if (!consumer.CreateProxy(
            kInstanceSpecifier,
            {"with_in_args_and_return", "with_in_args_only", "with_return_only", "without_args_or_return"}))
    {
        std::cerr << "Methods signature_variations consumer failed: CreateProxy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 2. Call method with InArgs and Return with copy
    std::cout << "\nConsumer: Step 2" << std::endl;
    if (!consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, MethodConsumer<MethodSignatureProxy>::CopyMode::WITH_COPY))
    {
        std::cerr << "Methods signature_variations consumer failed: CallMethodWithInArgsAndReturnWithCopy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 3. Call zero-copy method with InArgs and Return
    std::cout << "\nConsumer: Step 3" << std::endl;
    if (!consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, MethodConsumer<MethodSignatureProxy>::CopyMode::ZERO_COPY))
    {
        std::cerr << "Methods signature_variations consumer failed: CallMethodWithInArgsAndReturnZeroCopy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 4. Call method with InArgs only with copy
    std::cout << "\nConsumer: Step 4" << std::endl;
    if (!consumer.CallMethodWithInArgsOnly(kInArgOnlyMethodTestValueA,
                                           kInArgOnlyMethodTestValueB,
                                           MethodConsumer<MethodSignatureProxy>::CopyMode::WITH_COPY))
    {
        std::cerr << "Methods signature_variations consumer failed: CallMethodWithInArgsOnlyWithCopy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 5. Call zero-copy method with InArgs only
    std::cout << "\nConsumer: Step 5" << std::endl;
    if (!consumer.CallMethodWithInArgsOnly(kInArgOnlyMethodTestValueA,
                                           kInArgOnlyMethodTestValueB,
                                           MethodConsumer<MethodSignatureProxy>::CopyMode::ZERO_COPY))
    {
        std::cerr << "Methods signature_variations consumer failed: CallMethodWithInArgsOnlyZeroCopy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 6. Call method with return only with copy
    std::cout << "\nConsumer: Step 6" << std::endl;
    if (!consumer.CallMethodWithReturnOnly(kReturnOnlyMethodReturnValue))
    {
        std::cerr << "Methods signature_variations consumer failed: CallMethodWithReturnOnly" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 7. Call method without InArgs or return
    std::cout << "\nConsumer: Step 7" << std::endl;
    if (!consumer.CallMethodWithoutInArgsOrReturn())
    {
        std::cerr << "Methods signature_variations consumer failed: CallMethodWithoutInArgsOrReturn" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    process_synchronizer_result->Notify();

    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
