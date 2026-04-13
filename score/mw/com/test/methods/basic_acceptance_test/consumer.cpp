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

#include "score/mw/com/test/methods/basic_acceptance_test/test_method_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/method_consumer.h"
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

}  // namespace

int run_consumer()
{
    MethodConsumer<TestMethodProxy> consumer{};
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        std::cerr << "Methods basic_acceptance_test consumer failed: Could not create ProcessSynchronizer" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1" << std::endl;
    if (!consumer.CreateProxy(kInstanceSpecifier, {"with_in_args_and_return"}))
    {
        std::cerr << "Methods basic_acceptance_test consumer failed: CreateProxy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 2. Call method with copy
    std::cout << "\nConsumer: Step 2" << std::endl;
    if (!consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, MethodConsumer<TestMethodProxy>::CopyMode::WITH_COPY))
    {
        std::cerr << "Methods basic_acceptance_test consumer failed: CallMethodWithCopy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 3. Call zero-copy method
    std::cout << "\nConsumer: Step 3" << std::endl;
    if (!consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, MethodConsumer<TestMethodProxy>::CopyMode::ZERO_COPY))
    {
        std::cerr << "Methods basic_acceptance_test consumer failed: CallZeroCopyMethod" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    process_synchronizer_result->Notify();

    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
