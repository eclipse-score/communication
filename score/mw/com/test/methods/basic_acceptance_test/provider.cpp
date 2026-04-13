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

#include "score/mw/com/test/methods/basic_acceptance_test/test_method_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/method_provider.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
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

bool run_provider(const score::cpp::stop_token& stop_token)
{
    MethodProvider<TestMethodSkeleton> provider{};
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        std::cerr << "Methods basic_acceptance_test provider failed: Could not create ProcessSynchronizer" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 1. Create skeleton
    if (!provider.CreateSkeleton(kInstanceSpecifier))
    {
        std::cerr << "Methods basic_acceptance_test provider failed: CreateSkeleton" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 2. Register method handlers
    if (!provider.RegisterMethodHandlerWithInArgsAndReturn())
    {
        std::cerr << "Methods basic_acceptance_test provider failed: RegisterMethodHandlers" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 3. Offer service
    if (!provider.OfferService())
    {
        std::cerr << "Methods basic_acceptance_test provider failed: OfferService" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 4. Wait for proxy test to finish
    std::cout << "Provider: Ready for method calls" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        std::cerr << "Methods basic_acceptance_test provider failed: WaitForProxyTestToFinish was stopped by "
                     "stop_token instead of notification"
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Provider: Shutting down" << std::endl;
    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
