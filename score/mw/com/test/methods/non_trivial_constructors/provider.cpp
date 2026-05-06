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

#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/methods_test_resources/skeleton_container.h"
#include "score/mw/com/test/methods/non_trivial_constructors/test_method_datatype.h"

#include <score/stop_token.hpp>

#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{
const std::string kInterprocessNotificationShmPath{"/non_trivial_constructors_test_interprocess_notification"};

const std::string kInstanceSpecifier{"test/methods/non_trivial_constructors/MethodSignature"};

bool RegisterMethodHandlerWithInArgsAndReturn(NonTrivialConstructorSkeleton& skeleton)
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
        std::cerr << "Provider: Failed to register with_in_args_and_return handler" << std::endl;
        return false;
    }

    std::cout << "Provider: Successfully registered with_in_args_and_return handler" << std::endl;
    return true;
}

bool RegisterMethodHandlerWithInArgsOnly(NonTrivialConstructorSkeleton& skeleton)
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
        std::cerr << "Provider: Failed to register with_in_args_only handler" << std::endl;
        return false;
    }

    std::cout << "Provider: Successfully registered with_in_args_only handler" << std::endl;
    return true;
}

bool RegisterMethodHandlerWithReturnOnly(NonTrivialConstructorSkeleton& skeleton)
{
    auto handler_with_return_only = []() -> NonTriviallyConstructibleType {
        std::cout << "Provider: with_return_only called. Returning " << NonTriviallyConstructibleType{} << std::endl;
        return NonTriviallyConstructibleType{};
    };
    const auto register_result = skeleton.with_return_only.RegisterHandler(std::move(handler_with_return_only));
    if (!register_result)
    {
        std::cerr << "Provider: Failed to register with_return_only handler" << std::endl;
        return false;
    }

    std::cout << "Provider: Successfully registered with_return_only handler" << std::endl;
    return true;
}

}  // namespace

bool run_provider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        std::cerr << "Methods non_trivial_constructors provider failed: Could not create ProcessSynchronizer"
                  << std::endl;
        return EXIT_FAILURE;
    }

    SkeletonContainer<NonTrivialConstructorSkeleton> skeleton_container{kInstanceSpecifier};

    // Step 1. Create skeleton
    if (!skeleton_container.CreateSkeleton())
    {
        std::cerr << "Non trivial constructors provider failed: CreateSkeleton" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 2. Register method handler for method with InArgs and return value
    if (!RegisterMethodHandlerWithInArgsAndReturn(skeleton_container.GetSkeleton()))
    {
        std::cerr << "Non trivial constructors provider failed: RegisterMethodHandlerWithInArgsAndReturn" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 3. Register method handler for method with only InArgs
    if (!RegisterMethodHandlerWithInArgsOnly(skeleton_container.GetSkeleton()))
    {
        std::cerr << "Non trivial constructors provider failed: RegisterMethodHandlerWithInArgsOnly" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 4. Register method handler for method with only return value
    if (!RegisterMethodHandlerWithReturnOnly(skeleton_container.GetSkeleton()))
    {
        std::cerr << "Non trivial constructors provider failed: RegisterMethodHandlerWithReturnOnly" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 5. Offer service
    if (!skeleton_container.OfferService())
    {
        std::cerr << "Non trivial constructors provider failed: OfferService" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 6. Wait for proxy test to finish
    std::cout << "Provider: Ready for method calls" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        std::cerr << "Non trivial constructors provider failed: WaitForProxyTestToFinish was stopped by "
                     "stop_token instead of notification"
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Provider: Shutting down" << std::endl;
    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
