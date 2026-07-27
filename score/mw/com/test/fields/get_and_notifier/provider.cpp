/*******************************************************************************
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
 *******************************************************************************/

#include "score/mw/com/test/fields/get_and_notifier/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/fields/get_and_notifier/get_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/get_and_notifier/test_constants.h"

#include <iostream>

namespace score::mw::com::test
{

namespace
{

const InstanceSpecifier kInstanceSpecifier = InstanceSpecifier::Create(kInstanceSpecifierString).value();
const std::string kConsumerDoneShmPath{"/fields_get_and_notifier_consumer_done"};

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token)
{
    // Step 1. Create process synchronizer (waits for consumer to signal done)
    std::cout << "\nProvider: Step 1 - Create process synchronizer" << std::endl;
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create ProcessSynchronizer");
    }

    // Step 2. Create skeleton
    std::cout << "\nProvider: Step 2 - Create skeleton" << std::endl;
    SkeletonContainer<GetAndNotifierSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "get_and_notifier");

    auto& service = skeleton_container.GetSkeleton();

    // Step 3. Update field with initial value before offering so the consumer can subscribe
    //         and immediately receive it, and also can call Get()
    std::cout << "\nProvider: Step 3 - Update field with initial value (" << kInitialValue << ")" << std::endl;
    {
        const auto update_result = service.get_and_notifier_enabled_field.Update(kInitialValue);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with initial value: ", update_result.error());
        }
    }

    // Step 4. Offer service
    std::cout << "\nProvider: Step 4 - Offer service" << std::endl;
    skeleton_container.OfferService("get_and_notifier");

    // Step 5. Wait for consumer done notification
    std::cout << "\nProvider: Step 5 - Wait for consumer done notification" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort was stopped by stop_token instead of notification");
    }

    // Step 6. Stop offering service
    std::cout << "\nProvider: Step 6 - Stop offering service" << std::endl;
    service.StopOfferService();
}

}  // namespace score::mw::com::test
