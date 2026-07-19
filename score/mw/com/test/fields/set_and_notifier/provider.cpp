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

#include "score/mw/com/test/fields/set_and_notifier/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/fields/set_and_notifier/initial_only_field.h"
#include "score/mw/com/test/fields/set_and_notifier/set_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_and_notifier/test_constants.h"

#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kNotifierConsumerReadyShmPath{"/fields_notifier_consumer_ready"};
const std::string kNotifierConsumerDoneShmPath{"/fields_notifier_consumer_done"};
const std::string kSetAndNotifierConsumerDoneShmPath{"/fields_set_and_notifier_consumer_done"};

// InstanceSpecifier::Create can only fail if the provided string is invalid.
// Verified once here; all test functions reuse this constant.
const InstanceSpecifier kInstanceSpecifier = InstanceSpecifier::Create(std::string{kInstanceSpecifierString}).value();

void run_notifier_provider(const score::cpp::stop_token& stop_token)
{
    // Step 1. Create process synchronizers
    std::cout << "\nProvider: Step 1 - Create process synchronizers" << std::endl;
    auto consumer_ready_synchronizer_result = ProcessSynchronizer::Create(kNotifierConsumerReadyShmPath);
    if (!consumer_ready_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create consumer ready ProcessSynchronizer");
    }

    auto done_synchronizer_result = ProcessSynchronizer::Create(kNotifierConsumerDoneShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create done ProcessSynchronizer");
    }

    // Step 2. Create skeleton
    std::cout << "\nProvider: Step 2 - Create skeleton" << std::endl;
    SkeletonContainer<InitialOnlySkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "notifier");

    auto& service = skeleton_container.GetSkeleton();

    // Step 3. Update initial field value
    std::cout << "\nProvider: Step 3 - Update initial field value" << std::endl;
    {
        const auto update_result = service.initial_only_field.Update(kInitialValue);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update initial field value: ", update_result.error());
        }
    }

    // Step 4. Offer service
    std::cout << "\nProvider: Step 4 - Offer service" << std::endl;
    skeleton_container.OfferService("notifier");

    // Step 5. Wait until consumer is ready
    std::cout << "\nProvider: Step 5 - Wait for consumer ready notification" << std::endl;
    if (!consumer_ready_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (consumer ready) was stopped by stop_token instead of notification");
    }

    // Step 6. Update field with second value
    std::cout << "\nProvider: Step 6 - Update field with updated value" << std::endl;
    {
        const auto update_result = service.initial_only_field.Update(kUpdatedValue);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }

    // Step 7. Wait until consumer signals done
    std::cout << "\nProvider: Step 7 - Wait for consumer done notification" << std::endl;
    if (!done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (done) was stopped by stop_token instead of notification");
    }

    // Step 8. Stop offering service
    std::cout << "\nProvider: Step 8 - Stop offering service" << std::endl;
    service.StopOfferService();
}

void run_set_and_notifier_provider(const score::cpp::stop_token& stop_token)
{
    // Step 1. Create process synchronizer
    std::cout << "\nProvider: Step 1 - Create process synchronizer" << std::endl;
    auto process_synchronizer_result = ProcessSynchronizer::Create(kSetAndNotifierConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create ProcessSynchronizer");
    }

    // Step 2. Create skeleton
    std::cout << "\nProvider: Step 2 - Create skeleton" << std::endl;
    SkeletonContainer<SetAndNotifierEnabledSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "set_and_notifier");

    auto& service = skeleton_container.GetSkeleton();

    // Step 3. Register set handler
    std::cout << "\nProvider: Step 3 - Register set handler" << std::endl;
    const auto register_handler_result =
        service.set_and_notifier_enabled_field.RegisterSetHandler([](std::int32_t& value) noexcept {
            value = value * 2 + 1;
        });
    if (!register_handler_result.has_value())
    {
        FailTest("Provider: Unable to register set handler: ", register_handler_result.error());
    }

    // Step 4. Update initial field value
    std::cout << "\nProvider: Step 4 - Update initial field value" << std::endl;
    const auto update_result = service.set_and_notifier_enabled_field.Update(kInitialValue);
    if (!update_result.has_value())
    {
        FailTest("Provider: Unable to update initial field value: ", update_result.error());
    }

    // Step 5. Offer service
    std::cout << "\nProvider: Step 5 - Offer service" << std::endl;
    skeleton_container.OfferService("set_and_notifier");

    // Step 6. Wait for consumer done notification
    std::cout << "\nProvider: Step 6 - Wait for consumer done notification" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort was stopped by stop_token instead of notification");
    }

    // Step 7. Stop offering service
    std::cout << "\nProvider: Step 7 - Stop offering service" << std::endl;
    service.StopOfferService();
}

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token, const std::string& mode)
{
    if (mode == "notifier")
    {
        run_notifier_provider(stop_token);
        return;
    }
    if (mode == "set_and_notifier")
    {
        run_set_and_notifier_provider(stop_token);
        return;
    }

    // TODO: Add "get" mode provider scenario coverage once getter-enabled field variant is introduced.
    FailTest("Provider: Unsupported mode: ", mode);
}

}  // namespace score::mw::com::test
