/*******************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/
#include "score/mw/com/test/all_service_elements/provider.h"

#include "score/mw/com/test/all_service_elements/all_service_elements_datatype.h"
#include "score/mw/com/test/all_service_elements/test_constants.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"

#include <iostream>

namespace score::mw::com::test
{
namespace
{

template <typename SkeletonFieldType>
void UpdateField(SkeletonFieldType& field, const TestType value)
{
    const auto update_result = field.Update(value);
    if (!update_result.has_value())
    {
        FailTest(kFailureMessagePrefix, " Provider: Unable to update with value ", value, ": ", update_result.error());
    }
}

template <typename SkeletonEventType>
void SendEventSamplesOrFail(SkeletonEventType& skeleton_event, const std::vector<TestType>& values_to_send)
{
    for (const auto value : values_to_send)
    {
        const auto send_result = skeleton_event.Send(value);
        if (!send_result.has_value())
        {
            FailTest(kFailureMessagePrefix, " Provider: Unable to send with value ", value, ": ", send_result.error());
        }
    }
}

template <typename SkeletonFieldType>
void SendFieldSamplesOrFail(SkeletonFieldType& skeleton_field, const std::vector<TestType>& values_to_send)
{
    for (const auto value : values_to_send)
    {
        UpdateField(skeleton_field, value);
    }
}

void ValueTransformSetHandler(TestType& value) noexcept
{
    value = (value * 2) + 1;
}

void RegisterFieldHandlersOrFail(AllServiceElementsSkeleton& skeleton)
{
    const auto register_setter_and_notifier_result =
        skeleton.set_and_notifier_enabled_field.RegisterSetHandler([](TestType& value) noexcept {
            ValueTransformSetHandler(value);
        });
    if (!register_setter_and_notifier_result.has_value())
    {
        FailTest(kFailureMessagePrefix,
                 " Provider: Unable to register setter_and_notifier_field set handler: ",
                 register_setter_and_notifier_result.error());
    }

    const auto register_setter_and_getter_result =
        skeleton.set_and_get_enabled_field.RegisterSetHandler([](TestType& value) noexcept {
            ValueTransformSetHandler(value);
        });
    if (!register_setter_and_getter_result.has_value())
    {
        FailTest(kFailureMessagePrefix,
                 " Provider: Unable to register setter_and_getter_field set handler: ",
                 register_setter_and_getter_result.error());
    }

    const auto register_setter_getter_notifier_result =
        skeleton.set_and_get_and_notifier_enabled_field.RegisterSetHandler([](TestType& value) noexcept {
            ValueTransformSetHandler(value);
        });
    if (!register_setter_getter_notifier_result.has_value())
    {
        FailTest(kFailureMessagePrefix,
                 " Provider: Unable to register setter_getter_notifier_field set handler: ",
                 register_setter_getter_notifier_result.error());
    }
}

void RegisterMethodHandlersOrFail(AllServiceElementsSkeleton& skeleton)
{
    auto handler_with_in_args_and_return = [](TestType& return_value, const TestType& a, const TestType& b) {
        std::cout << "Provider: with_in_args_and_return called with " << a << " + " << b << std::endl;
        return_value = a + b;
    };
    const auto register_in_args_and_return_result =
        skeleton.with_in_args_and_return.RegisterHandler(std::move(handler_with_in_args_and_return));
    if (!register_in_args_and_return_result)
    {
        FailTest(kFailureMessagePrefix, " Provider: Failed to register with_in_args_and_return handler");
    }

    auto handler_with_in_args_only = [](const TestType& a, const TestType& b) {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(a == kInArgOnlyMethodTestValueA, "Unexpected first InArg received!");
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(b == kInArgOnlyMethodTestValueB, "Unexpected second InArg received!");
    };
    const auto register_with_in_args_only_result =
        skeleton.with_in_args_only.RegisterHandler(std::move(handler_with_in_args_only));
    if (!register_with_in_args_only_result)
    {
        FailTest(kFailureMessagePrefix, " Provider: Failed to register with_in_args_only handler");
    }

    auto handler_with_return_only = [](TestType& return_value) {
        return_value = kReturnOnlyMethodReturnValue;
    };
    const auto register_with_return_only_result =
        skeleton.with_return_only.RegisterHandler(std::move(handler_with_return_only));
    if (!register_with_return_only_result)
    {
        FailTest(kFailureMessagePrefix, " Provider: Failed to register with_return_only handler");
    }

    auto handler_without_in_args_or_return = []() {};
    const auto register_without_in_args_or_return_result =
        skeleton.without_args_or_return.RegisterHandler(std::move(handler_without_in_args_or_return));
    if (!register_without_in_args_or_return_result)
    {
        FailTest(kFailureMessagePrefix, " Provider: Failed to register without_args_or_return handler");
    }
}

}  // namespace

void RunProvider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest(kFailureMessagePrefix, " Provider: Could not create ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<AllServiceElementsSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, kFailureMessagePrefix);
    auto& service = skeleton_container.GetSkeleton();

    // Step 2. Register field handlers
    std::cout << "\nProvider: Step 2 - Register field handlers" << std::endl;
    RegisterFieldHandlersOrFail(service);

    // Step 3. Register method handlers
    std::cout << "\nProvider: Step 3 - Register method handlers" << std::endl;
    RegisterMethodHandlersOrFail(service);

    // Step 4. Set initial value for all fields
    std::cout << "\nProvider: Step 4 - Set initial field values" << std::endl;
    UpdateField(service.get_only_enabled_field, kGetOnlyFieldInitialValue);
    UpdateField(service.set_and_get_enabled_field, kSetAndGetFieldInitialValue);
    UpdateField(service.get_and_notifier_enabled_field, kGetAndNotifierFieldInitialValue);
    UpdateField(service.set_and_notifier_enabled_field, kSetAndNotifierFieldInitialValue);
    UpdateField(service.set_and_get_and_notifier_enabled_field, kSetAndGetAndNotifierFieldInitialValue);
    UpdateField(service.notifier_only_enabled_field, kNotifierOnlyFieldInitialValue);

    // Step 5. Offer service
    std::cout << "\nProvider: Step 5 - Offer service" << std::endl;
    skeleton_container.OfferService(kFailureMessagePrefix);

    // Step 6. Send event and field samples
    std::cout << "\nProvider: Step 6 - Send event and field samples" << std::endl;
    SendEventSamplesOrFail(service.event_1, kEvent1ValuesToSend);
    SendEventSamplesOrFail(service.event_2, kEvent2ValuesToSend);
    SendFieldSamplesOrFail(service.get_and_notifier_enabled_field, kGetAndNotifierFieldValuesToSend);
    SendFieldSamplesOrFail(service.set_and_notifier_enabled_field, kSetAndNotifierFieldValuesToSend);
    SendFieldSamplesOrFail(service.set_and_get_and_notifier_enabled_field, kSetAndGetAndNotifierFieldValuesToSend);
    SendFieldSamplesOrFail(service.notifier_only_enabled_field, kNotifierOnlyFieldValuesToSend);

    // Step 7. Wait for consumer to finish
    std::cout << "\nProvider: Step 7 - Wait for consumer done notification" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(kFailureMessagePrefix, " Provider: WaitWithAbort was stopped by stop_token instead of notification");
    }
}

}  // namespace score::mw::com::test
