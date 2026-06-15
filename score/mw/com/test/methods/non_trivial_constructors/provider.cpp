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
#include "score/mw/com/test/methods/non_trivial_constructors/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/methods/non_trivial_constructors/test_method_datatype.h"

#include "score/mw/com/types.h"
#include <score/stop_token.hpp>

#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{
const std::string kInterprocessNotificationShmPath{"/non_trivial_constructors_test_interprocess_notification"};

const auto kInstanceSpecifier{
    InstanceSpecifier::Create(std::string{"test/methods/non_trivial_constructors/MethodSignature"}).value()};

void RegisterMethodHandlerWithInArgsAndReturn(NonTrivialConstructorSkeleton& skeleton,
                                              const std::string& failure_message_prefix)
{
    auto handler_with_in_args_and_return = [](NonTriviallyConstructibleType a,
                                              NonTriviallyConstructibleType b) -> NonTriviallyConstructibleType {
        std::cout << "Provider: with_in_args_and_return called with " << a << " + " << b << std::endl;
        return a + b;
    };
    const auto register_result =
        skeleton.with_in_args_and_return.RegisterHandler(std::move(handler_with_in_args_and_return));
    if (!register_result)
    {
        FailTest(failure_message_prefix, " Provider: Failed to register with_in_args_and_return handler");
    }

    std::cout << "Provider: Successfully registered with_in_args_and_return handler" << std::endl;
}

void RegisterMethodHandlerWithInArgsOnly(NonTrivialConstructorSkeleton& skeleton,
                                         const std::string& failure_message_prefix)
{
    auto handler_with_in_args_only = [](NonTriviallyConstructibleType a, NonTriviallyConstructibleType b) {
        std::cout << "Provider: with_in_args_only called with " << a << " + " << b << std::endl;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(a == NonTriviallyConstructibleType{},
                                                "Unexpected first InArg received!");
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(b == NonTriviallyConstructibleType{},
                                                "Unexpected second InArg received!");
    };
    const auto register_result = skeleton.with_in_args_only.RegisterHandler(std::move(handler_with_in_args_only));
    if (!register_result)
    {
        FailTest(failure_message_prefix, " Provider: Failed to register with_in_args_only handler");
    }

    std::cout << "Provider: Successfully registered with_in_args_only handler" << std::endl;
}

void RegisterMethodHandlerWithReturnOnly(NonTrivialConstructorSkeleton& skeleton,
                                         const std::string& failure_message_prefix)
{
    auto handler_with_return_only = []() -> NonTriviallyConstructibleType {
        std::cout << "Provider: with_return_only called. Returning " << NonTriviallyConstructibleType{} << std::endl;
        return NonTriviallyConstructibleType{};
    };
    const auto register_result = skeleton.with_return_only.RegisterHandler(std::move(handler_with_return_only));
    if (!register_result)
    {
        FailTest(failure_message_prefix, " Provider: Failed to register with_return_only handler");
    }

    std::cout << "Provider: Successfully registered with_return_only handler" << std::endl;
}

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("Methods non_trivial_constructors provider failed: Could not create ProcessSynchronizer");
    }

    SkeletonContainer<NonTrivialConstructorSkeleton> skeleton_container{};

    // Step 1. Create skeleton
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "non_trivial_constructors");

    // Step 2. Register method handler for method with InArgs and return value
    RegisterMethodHandlerWithInArgsAndReturn(skeleton_container.GetSkeleton(), "non_trivial_constructors");

    // Step 3. Register method handler for method with only InArgs
    RegisterMethodHandlerWithInArgsOnly(skeleton_container.GetSkeleton(), "non_trivial_constructors");

    // Step 4. Register method handler for method with only return value
    RegisterMethodHandlerWithReturnOnly(skeleton_container.GetSkeleton(), "non_trivial_constructors");

    // Step 5. Offer service
    skeleton_container.OfferService("non_trivial_constructors");

    // Step 6. Wait for proxy test to finish
    std::cout << "Provider: Ready for method calls" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(
            "Non trivial constructors provider failed: WaitForProxyTestToFinish was stopped by "
            "stop_token instead of notification");
        return;
    }

    std::cout << "Provider: Shutting down" << std::endl;
}

}  // namespace score::mw::com::test
