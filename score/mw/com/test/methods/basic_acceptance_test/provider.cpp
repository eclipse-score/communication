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
#include "score/mw/com/test/methods/basic_acceptance_test/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/basic_acceptance_test/test_method_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/methods_test_resources/skeleton_container.h"
#include "score/mw/com/types.h"

#include "score/result/result.h"

#include <score/stop_token.hpp>

#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/basic_acc_test_interprocess_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/basic_acceptance_test/TestMethods"}).value();

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token)
{
    SkeletonContainer<TestMethodSkeleton> skeleton_container{};
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("Methods basic_acceptance_test provider failed: Could not create ProcessSynchronizer");
        return;
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1" << std::endl;
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "basic_acceptance_test");
    auto& skeleton = skeleton_container.GetSkeleton();

    // Step 2. Register method handler
    std::cout << "\nProvider: Step 2" << std::endl;
    auto handler_with_in_args_and_return = [](std::int32_t a, std::int32_t b) -> std::int32_t {
        std::cout << "Provider: with_in_args_and_return called with " << a << " + " << b << std::endl;
        return a + b;
    };
    const auto register_result =
        skeleton.with_in_args_and_return.RegisterHandler(std::move(handler_with_in_args_and_return));
    if (!register_result)
    {
        FailTest("Provider: Failed to register with_in_args_and_return handler");
        return;
    }

    // Step 3. Offer service
    std::cout << "\nProvider: Step 3" << std::endl;
    skeleton_container.OfferService("basic_acceptance_test");

    // Step 4. Wait for proxy test to finish
    std::cout << "\nProvider: Step 4" << std::endl;
    std::cout << "Provider: Ready for method calls" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(
            "Methods basic_acceptance_test provider failed: WaitForProxyTestToFinish was stopped by "
            "stop_token instead of notification");
        return;
    }

    std::cout << "Provider: Shutting down" << std::endl;
}

}  // namespace score::mw::com::test
