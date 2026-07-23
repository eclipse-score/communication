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
#include "score/mw/com/test/methods/mixed_criticality/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_method_provider.h"
#include "score/mw/com/types.h"

#include "score/result/result.h"

#include <score/stop_token.hpp>

#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{
const std::string kInterprocessNotificationShmPath{"/mixed_criticality_test_interprocess_notification"};
const std::string kFailureMessagePrefix{"mixed_criticality"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/mixed_criticality/MethodSignature"}).value();

const std::int32_t kReturnOnlyMethodReturnValue{15};
const std::int32_t kInArgOnlyMethodTestValueA{17};
const std::int32_t kInArgOnlyMethodTestValueB{18};

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token)
{
    AllSignaturesMethodProvider provider{};
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("Methods mixed_criticality provider failed: Could not create ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1" << std::endl;
    provider.CreateSkeleton(kInstanceSpecifier, kFailureMessagePrefix);

    std::cout << "Provider: Ready for method calls from multiple proxies\n";

    // Step 2. Register method handlers
    std::cout << "\nProvider: Step 2" << std::endl;
    provider.RegisterMethodHandlerWithInArgsAndReturn(kFailureMessagePrefix);
    provider.RegisterMethodHandlerWithInArgsOnly(
        kInArgOnlyMethodTestValueA, kInArgOnlyMethodTestValueB, kFailureMessagePrefix);
    provider.RegisterMethodHandlerWithReturnOnly(kReturnOnlyMethodReturnValue, kFailureMessagePrefix);
    provider.RegisterWithoutInArgsOrReturn(kFailureMessagePrefix);

    // Step 3. Offer service
    std::cout << "\nProvider: Step 3" << std::endl;
    provider.OfferService(kFailureMessagePrefix);

    // Step 4. Wait for proxy test to finish
    std::cout << "Provider: Ready for method calls" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(
            "Methods mixed_criticality provider failed: WaitForProxyTestToFinish was stopped by "
            "stop_token instead of notification");
    }

    std::cout << "Provider: Shutting down.\n";
}

}  // namespace score::mw::com::test
