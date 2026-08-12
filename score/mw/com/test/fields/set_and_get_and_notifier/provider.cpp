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

#include "score/mw/com/test/fields/set_and_get_and_notifier/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/get_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/getter_only_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/notifier_only_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/set_and_get_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/set_and_get_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/datatypes/set_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_and_get_and_notifier/test_constants.h"

#include <iostream>

namespace score::mw::com::test
{
namespace
{

void ValueTransformSetHandler(std::int32_t& value) noexcept
{
    value = (value * 2) + 1;
}

template <typename FieldType>
void UpdateInitialValue(FieldType& field)
{
    const auto update_result = field.Update(kInitialValue);
    if (!update_result.has_value())
    {
        FailTest("Provider: Unable to update field with initial value: ", update_result.error());
    }
}

void run_notifier_provider(const score::cpp::stop_token& stop_token)
{
    auto done_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create done ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<NotifierOnlyEnabledSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "notifier");

    auto& service = skeleton_container.GetSkeleton();

    // Step 2. Update initial field value
    std::cout << "\nProvider: Step 2 - Update initial field value" << std::endl;
    UpdateInitialValue(service.notifier_only_enabled_field);

    // Step 3. Offer service
    std::cout << "\nProvider: Step 3 - Offer service" << std::endl;
    skeleton_container.OfferService("notifier");

    // Step 4. Update field with updated value
    std::cout << "\nProvider: Step 4 - Update field with updated value" << std::endl;
    const std::vector<std::int32_t> values_to_send = {20, 30, 35};
    for (auto value_to_send : values_to_send)
    {
        const auto update_result = service.notifier_only_enabled_field.Update(value_to_send);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }

    // Step 5. Wait until consumer signals done
    std::cout << "\nProvider: Step 5 - Wait for consumer done notification" << std::endl;
    if (!done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (done) was stopped by stop_token instead of notification");
    }
}

void run_set_and_notifier_provider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create ProcessSynchronizer");
    }
    auto set_done_synchronizer_result = ProcessSynchronizer::Create(kSetDoneShmPath);
    if (!set_done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create set-done ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<SetAndNotifierEnabledSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "set_and_notifier");

    auto& service = skeleton_container.GetSkeleton();

    // Step 2. Register set handler
    std::cout << "\nProvider: Step 2 - Register set handler" << std::endl;
    const auto register_handler_result =
        service.set_and_notifier_enabled_field.RegisterSetHandler([](std::int32_t& value) noexcept {
            ValueTransformSetHandler(value);
        });
    if (!register_handler_result.has_value())
    {
        FailTest("Provider: Unable to register set handler: ", register_handler_result.error());
    }

    // Step 3. Update initial field value
    std::cout << "\nProvider: Step 3 - Update initial field value" << std::endl;
    UpdateInitialValue(service.set_and_notifier_enabled_field);

    // Step 4. Offer service
    std::cout << "\nProvider: Step 4 - Offer service" << std::endl;
    skeleton_container.OfferService("set_and_notifier");

    // Step 5. Update field with updated values
    std::cout << "\nProvider: Step 5 - Update field with updated value" << std::endl;
    const std::vector<std::int32_t> first_values_to_send = {20, 30, 35};
    for (auto value_to_send : first_values_to_send)
    {
        const auto update_result = service.set_and_notifier_enabled_field.Update(value_to_send);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }

    // Step 6. Wait for consumer to finish Set() and verify the accepted value
    std::cout << "\nProvider: Step 6 - Wait for consumer set completion" << std::endl;
    if (!set_done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (set-done) was stopped by stop_token");
    }

    // Step 7. Update field with updated values
    std::cout << "\nProvider: Step 7 - Update field with updated value" << std::endl;
    const std::vector<std::int32_t> second_values_to_send = {10, 100};
    for (auto value_to_send : second_values_to_send)
    {
        const auto update_result = service.set_and_notifier_enabled_field.Update(value_to_send);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }

    // Step 8. Wait for consumer done notification
    std::cout << "\nProvider: Step 8 - Wait for consumer done notification" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort was stopped by stop_token instead of notification");
    }
}

void run_get_provider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create ProcessSynchronizer");
    }
    auto got_value_process_synchronizer_result = ProcessSynchronizer::Create(kConsumerGotValueShmPath);
    if (!got_value_process_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create GotValue ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<GetterOnlyEnabledSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "get");
    auto& service = skeleton_container.GetSkeleton();

    // Step 2. Update field with initial value so the consumer's Get() reads it
    std::cout << "\nProvider: Step 2 - Update field with initial value (" << kInitialValue << ")" << std::endl;
    UpdateInitialValue(service.getter_only_enabled_field);

    // Step 3. Offer service
    std::cout << "\nProvider: Step 3 - Offer service" << std::endl;
    skeleton_container.OfferService("get");

    // Step 4. Wait for consumer to signal that it has called Get() and verified the value
    std::cout << "\nProvider: Step 4 - Wait for consumer to signal that it has called Get() and verified the value"
              << std::endl;
    if (!got_value_process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (got value) was stopped by stop_token instead of notification");
    }

    // Step 5. Update field with updated value so the consumer's Get() reads it
    std::cout << "\nProvider: Step 5 - Update field with updated value (" << kUpdatedValue << ")" << std::endl;
    const auto update_result = service.getter_only_enabled_field.Update(kUpdatedValue);
    if (!update_result.has_value())
    {
        FailTest("Provider: Unable to update field with updated value: ", update_result.error());
    }

    // Step 6. Wait for consumer done notification
    std::cout << "\nProvider: Step 6 - Wait for consumer done notification" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort was stopped by stop_token instead of notification");
    }
}

void run_get_and_notifier_provider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create ProcessSynchronizer");
    }
    auto got_value_process_synchronizer_result = ProcessSynchronizer::Create(kConsumerGotValueShmPath);
    if (!got_value_process_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create GotValue ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<GetAndNotifierEnabledSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "get_and_notifier");
    auto& service = skeleton_container.GetSkeleton();

    // Step 2. Update field with initial value before offering so the consumer can subscribe, receive via notifier, and
    // also call Get()
    std::cout << "\nProvider: Step 2 - Update field with initial value (" << kInitialValue << ")" << std::endl;
    UpdateInitialValue(service.get_and_notifier_enabled_field);

    // Step 3. Offer service
    std::cout << "\nProvider: Step 3 - Offer service" << std::endl;
    skeleton_container.OfferService("get_and_notifier");

    // Step 4. Update field with updated values
    std::cout << "\nProvider: Step 4 - Update field with updated value" << std::endl;
    const std::vector<std::int32_t> first_values_to_send = {20, 30, 35};
    for (auto value_to_send : first_values_to_send)
    {
        const auto update_result = service.get_and_notifier_enabled_field.Update(value_to_send);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }

    // Step 5. Wait for consumer to signal that it has called Get() and verified the value
    std::cout << "\nProvider: Step 5 - Wait for consumer to signal that it has called Get() and verified the value"
              << std::endl;
    if (!got_value_process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (got value) was stopped by stop_token instead of notification");
    }

    // Step 6. Update field with updated values
    std::cout << "\nProvider: Step 6 - Update field with updated value" << std::endl;
    const std::vector<std::int32_t> second_values_to_send = {10, 100};
    for (auto value_to_send : second_values_to_send)
    {
        const auto update_result = service.get_and_notifier_enabled_field.Update(value_to_send);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }

    // Step 7. Wait for consumer done notification
    std::cout << "\nProvider: Step 7 - Wait for consumer done notification" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort was stopped by stop_token instead of notification");
    }
}

void run_set_and_get_provider(const score::cpp::stop_token& stop_token)
{
    auto consumer_done_sync_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!consumer_done_sync_result.has_value())
    {
        FailTest("Provider: Could not create consumer-done ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<SetAndGetEnabledSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "set_and_get");
    auto& service = skeleton_container.GetSkeleton();

    // Step 2. Register set handler
    std::cout << "\nProvider: Step 2 - Register set handler" << std::endl;
    const auto register_handler_result = service.set_and_get_enabled_field.RegisterSetHandler([](std::int32_t& value) {
        ValueTransformSetHandler(value);
    });
    if (!register_handler_result.has_value())
    {
        FailTest("Provider: Unable to register set handler: ", register_handler_result.error());
    }

    // Step 3. Update field with initial value
    std::cout << "\nProvider: Step 3 - Update field with initial value (" << kInitialValue << ")" << std::endl;
    UpdateInitialValue(service.set_and_get_enabled_field);

    // Step 4. Offer service
    std::cout << "\nProvider: Step 4 - Offer service" << std::endl;
    skeleton_container.OfferService("set_and_get");

    // Step 5. Wait for consumer done notification
    std::cout << "\nProvider: Step 5 - Wait for consumer done notification" << std::endl;
    if (!consumer_done_sync_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (consumer-done) was stopped by stop_token");
    }
}

void run_set_and_get_and_notifier_provider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create consumer-done ProcessSynchronizer");
    }
    auto set_done_synchronizer_result = ProcessSynchronizer::Create(kSetDoneShmPath);
    if (!set_done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create set-done ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<SetAndGetAndNotifierEnabledSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "set_get_and_notifier");
    auto& service = skeleton_container.GetSkeleton();

    // Step 2. Register set handler
    std::cout << "\nProvider: Step 2 - Register set handler" << std::endl;
    const auto register_handler_result =
        service.set_and_get_and_notifier_enabled_field.RegisterSetHandler([](std::int32_t& value) noexcept {
            ValueTransformSetHandler(value);
        });
    if (!register_handler_result.has_value())
    {
        FailTest("Provider: Unable to register set handler: ", register_handler_result.error());
    }

    // Step 3. Update field with initial value
    std::cout << "\nProvider: Step 3 - Update field with initial value (" << kInitialValue << ")" << std::endl;
    UpdateInitialValue(service.set_and_get_and_notifier_enabled_field);

    // Step 4. Offer service
    std::cout << "\nProvider: Step 4 - Offer service" << std::endl;
    skeleton_container.OfferService("set_get_and_notifier");

    // Step 5. Update field with updated value
    std::cout << "\nProvider: Step 5 - Update field with updated value" << std::endl;
    const std::vector<std::int32_t> values_to_send = {20, 30, 35};
    for (auto value_to_send : values_to_send)
    {
        const auto update_result = service.set_and_get_and_notifier_enabled_field.Update(value_to_send);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }

    // Step 6. Wait for consumer to finish Set() and verify the accepted value
    std::cout << "\nProvider: Step 6 - Wait for consumer set completion" << std::endl;
    if (!set_done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (set-done) was stopped by stop_token");
    }

    // Step 7. Update field with updated values
    std::cout << "\nProvider: Step 7 - Update field with updated value" << std::endl;
    {
        const std::vector<std::int32_t> values_to_send = {10, 100};
        for (auto value_to_send : values_to_send)
        {
            const auto update_result = service.set_and_get_and_notifier_enabled_field.Update(value_to_send);
            if (!update_result.has_value())
            {
                FailTest("Provider: Unable to update field with updated value: ", update_result.error());
            }
        }
    }

    // Step 8. Wait for consumer done notification
    std::cout << "\nProvider: Step 8 - Wait for consumer done notification" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (consumer-done) was stopped by stop_token");
    }
}

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token, TestMode mode)
{
    switch (mode)
    {
        case TestMode::kNotifier:
            run_notifier_provider(stop_token);
            return;
        case TestMode::kSetAndNotifier:
            run_set_and_notifier_provider(stop_token);
            return;
        case TestMode::kGet:
            run_get_provider(stop_token);
            return;
        case TestMode::kGetAndNotifier:
            run_get_and_notifier_provider(stop_token);
            return;
        case TestMode::kSetAndGet:
            run_set_and_get_provider(stop_token);
            return;
        case TestMode::kSetAndGetAndNotifier:
            run_set_and_get_and_notifier_provider(stop_token);
            return;
    }
}

}  // namespace score::mw::com::test
