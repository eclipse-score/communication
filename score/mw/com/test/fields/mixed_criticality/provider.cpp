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
#include "score/mw/com/test/fields/mixed_criticality/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/fields/mixed_criticality/common_resources.h"
#include "score/mw/com/test/fields/mixed_criticality/datatypes/mixed_criticality_fields.h"

#include <cstdint>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

template <typename SkeletonFieldType>
void UpdateField(SkeletonFieldType& field, const std::int32_t value, const char* const field_name)
{
    const auto update_result = field.Update(value);
    if (!update_result.has_value())
    {
        FailTest("Provider: Unable to update ", field_name, " with value ", value, ": ", update_result.error());
    }
}

void ValueTransformSetHandler(std::int32_t& value) noexcept
{
    value = (value * 2) + 1;
}

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Fields mixed_criticality provider failed: Could not create ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<MixedCriticalityFieldsSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, kFailureMessagePrefix);
    auto& service = skeleton_container.GetSkeleton();

    // Step 2. Register set handlers
    std::cout << "\nProvider: Step 2 - Register set handlers" << std::endl;
    const auto register_setter_and_notifier_result =
        service.setter_and_notifier_field.RegisterSetHandler([](std::int32_t& value) noexcept {
            ValueTransformSetHandler(value);
        });
    if (!register_setter_and_notifier_result.has_value())
    {
        FailTest("Provider: Unable to register setter_and_notifier_field set handler: ",
                 register_setter_and_notifier_result.error());
    }

    const auto register_setter_and_getter_result =
        service.setter_and_getter_field.RegisterSetHandler([](std::int32_t& value) noexcept {
            ValueTransformSetHandler(value);
        });
    if (!register_setter_and_getter_result.has_value())
    {
        FailTest("Provider: Unable to register setter_and_getter_field set handler: ",
                 register_setter_and_getter_result.error());
    }

    const auto register_setter_getter_notifier_result =
        service.setter_getter_notifier_field.RegisterSetHandler([](std::int32_t& value) noexcept {
            ValueTransformSetHandler(value);
        });
    if (!register_setter_getter_notifier_result.has_value())
    {
        FailTest("Provider: Unable to register setter_getter_notifier_field set handler: ",
                 register_setter_getter_notifier_result.error());
    }

    // Step 3. Set initial value for all fields
    std::cout << "\nProvider: Step 3 - Set initial field values" << std::endl;
    UpdateField(service.getter_only_field, kInitialValue, "getter_only_field");
    UpdateField(service.setter_and_getter_field, kInitialValue, "setter_and_getter_field");
    UpdateField(service.getter_and_notifier_field, kInitialValue, "getter_and_notifier_field");
    UpdateField(service.setter_and_notifier_field, kInitialValue, "setter_and_notifier_field");
    UpdateField(service.setter_getter_notifier_field, kInitialValue, "setter_getter_notifier_field");
    UpdateField(service.notifier_only_field, kInitialValue, "notifier_only_field");

    // Step 4. Offer service
    std::cout << "\nProvider: Step 4 - Offer service" << std::endl;
    skeleton_container.OfferService(kFailureMessagePrefix);

    // Step 5. Send four more samples per field
    std::cout << "\nProvider: Step 5 - Send follow-up field samples" << std::endl;
    for (const auto value : kFollowupValues)
    {
        UpdateField(service.getter_only_field, value, "getter_only_field");
        UpdateField(service.setter_and_getter_field, value, "setter_and_getter_field");
        UpdateField(service.getter_and_notifier_field, value, "getter_and_notifier_field");
        UpdateField(service.setter_and_notifier_field, value, "setter_and_notifier_field");
        UpdateField(service.setter_getter_notifier_field, value, "setter_getter_notifier_field");
        UpdateField(service.notifier_only_field, value, "notifier_only_field");
    }

    // Step 6. Wait for consumer to finish
    std::cout << "\nProvider: Step 6 - Wait for consumer done notification" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(
            "Fields mixed_criticality provider failed: WaitWithAbort was stopped by stop_token instead of "
            "notification");
    }
}

}  // namespace score::mw::com::test
