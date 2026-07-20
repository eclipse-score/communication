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
#include "score/mw/com/test/methods/semi_dynamic_methods/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/methods/semi_dynamic_methods/runtime_sized_array.h"
#include "score/mw/com/test/methods/semi_dynamic_methods/semi_dynamic_method_datatype.h"
#include "score/mw/com/types.h"

#include "score/result/result.h"

#include <score/stop_token.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/semi_dynamic_methods_test_interprocess_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/semi_dynamic_methods/TestMethods"}).value();

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token)
{
    SkeletonContainer<SemiDynamicMethodSkeleton> skeleton_container{};
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("Methods semi_dynamic_methods_test provider failed: Could not create ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "semi_dynamic_methods_test");
    auto& skeleton = skeleton_container.GetSkeleton();

    // Step 2. Register method handler
    std::cout << "\nProvider: Step 2 - Register method handler" << std::endl;
    auto handler_with_in_args_and_return = [](SemiDynamicMethodSkeleton::SemiDynamicArray& return_value,
                                              const SemiDynamicMethodSkeleton::SemiDynamicArray& in_arg) {
        const auto in_arg_size = in_arg.size();
        // Reserve the return_value capacity only if it is currently zero (i.e. the first time the method is called).
        // All subsequent calls will reuse the already allocated capacity of the return_value.
        if (return_value.capacity() == 0U)
        {
            return_value.ReserveOnce(in_arg_size);
            for (std::size_t i = 0; i < in_arg_size; ++i)
            {
                return_value.emplace_back(in_arg.at(i) * 2U);
            }
        }
        else
        {
            for (std::size_t i = 0; i < in_arg_size; ++i)
            {
                return_value.at(i) = in_arg.at(i) * 2U;
            }
        }
    };
    const auto register_result =
        skeleton.with_in_args_and_return.RegisterHandler(std::move(handler_with_in_args_and_return));
    if (!register_result)
    {
        FailTest("Provider: Failed to register with_in_args_and_return handler");
    }

    // Step 3. Offer service
    std::cout << "\nProvider: Step 3 - Offer Service" << std::endl;
    skeleton_container.OfferService("semi_dynamic_methods_test");

    // Step 4. Wait for proxy test to finish
    std::cout << "\nProvider: Step 4 - Wait for proxy to finish" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(
            "Methods semi_dynamic_methods_test provider failed: WaitForProxyTestToFinish was stopped by "
            "stop_token instead of notification");
    }

    std::cout << "Provider: Shutting down" << std::endl;
}

}  // namespace score::mw::com::test
