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

#include "score/mw/com/test/fields/set_and_get/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/fields/set_and_get/set_and_get_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get/test_constants.h"

#include <algorithm>
#include <iostream>

namespace score::mw::com::test
{

namespace
{

const InstanceSpecifier kInstanceSpecifier = InstanceSpecifier::Create(kInstanceSpecifierString).value();
const std::string kSetsDoneShmPath{"/fields_set_and_get_sets_done"};
const std::string kProviderUpdatedShmPath{"/fields_set_and_get_provider_updated"};
const std::string kConsumerDoneShmPath{"/fields_set_and_get_consumer_done"};

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token)
{
    // Step 1. Create process synchronizers
    std::cout << "\nProvider: Step 1 - Create process synchronizers" << std::endl;
    auto sets_done_sync_result = ProcessSynchronizer::Create(kSetsDoneShmPath);
    if (!sets_done_sync_result.has_value())
    {
        FailTest("Provider: Could not create sets-done ProcessSynchronizer");
    }
    auto provider_updated_sync_result = ProcessSynchronizer::Create(kProviderUpdatedShmPath);
    if (!provider_updated_sync_result.has_value())
    {
        FailTest("Provider: Could not create provider-updated ProcessSynchronizer");
    }
    auto consumer_done_sync_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!consumer_done_sync_result.has_value())
    {
        FailTest("Provider: Could not create consumer-done ProcessSynchronizer");
    }

    // Step 2. Create skeleton
    std::cout << "\nProvider: Step 2 - Create skeleton" << std::endl;
    SkeletonContainer<SetAndGetSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "set_and_get");
    auto& service = skeleton_container.GetSkeleton();

    // Step 3. Register set handler — clamps incoming value to [kMinFieldValue, kMaxFieldValue]
    std::cout << "\nProvider: Step 3 - Register set handler (clamp to [" << kMinFieldValue << ", " << kMaxFieldValue
              << "])" << std::endl;
    service.set_and_get_enabled_field.RegisterSetHandler([](const std::int32_t& requested_value) -> std::int32_t {
        return std::max(kMinFieldValue, std::min(kMaxFieldValue, requested_value));
    });

    // Step 4. Update field with initial value
    std::cout << "\nProvider: Step 4 - Update field with initial value (" << kInitialValue << ")" << std::endl;
    {
        const auto update_result = service.set_and_get_enabled_field.Update(kInitialValue);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with initial value: ", update_result.error());
        }
    }

    // Step 5. Offer service
    std::cout << "\nProvider: Step 5 - Offer service" << std::endl;
    skeleton_container.OfferService("set_and_get");

    // Step 6. Wait for consumer to finish all Set() operations
    std::cout << "\nProvider: Step 6 - Wait for consumer sets-done notification" << std::endl;
    if (!sets_done_sync_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (sets-done) was stopped by stop_token");
    }

    // Step 7. Update field with new value so consumer can verify Get() returns latest value
    std::cout << "\nProvider: Step 7 - Update field with updated value (" << kUpdatedValue << ")" << std::endl;
    {
        const auto update_result = service.set_and_get_enabled_field.Update(kUpdatedValue);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }

    // Step 8. Notify consumer that the field has been updated
    std::cout << "\nProvider: Step 8 - Notify consumer that field has been updated" << std::endl;
    provider_updated_sync_result->Notify();

    // Step 9. Wait for consumer done notification
    std::cout << "\nProvider: Step 9 - Wait for consumer done notification" << std::endl;
    if (!consumer_done_sync_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (consumer-done) was stopped by stop_token");
    }

    // Step 10. Stop offering service
    std::cout << "\nProvider: Step 10 - Stop offering service" << std::endl;
    service.StopOfferService();
}

}  // namespace score::mw::com::test
