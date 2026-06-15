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

#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/field_provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/initial_only_field.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/set_enabled_field.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/test_constants.h"

#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/fields_set_and_notifier_interprocess_notification"};
const std::string kNotifierConsumerReadyShmPath{"/fields_notifier_consumer_ready"};

void run_notifier_provider(const score::cpp::stop_token& stop_token)
{
    auto consumer_ready_synchronizer_result = ProcessSynchronizer::Create(kNotifierConsumerReadyShmPath);
    if (!consumer_ready_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create consumer ready ProcessSynchronizer");
    }

    auto done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create done ProcessSynchronizer");
    }

    SkeletonContainer<InitialOnlySkeleton> skeleton_container{kInstanceSpecifierString};
    if (!skeleton_container.CreateSkeleton())
    {
        FailTest("Provider: Unable to construct InitialOnlySkeleton");
    }

    auto& service = skeleton_container.GetSkeleton();

    {
        const auto update_result = service.test_field.Update(kInitialValue);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update initial field value: ", update_result.error());
        }
    }
    if (!skeleton_container.OfferService())
    {
        FailTest("Provider: Unable to offer InitialOnlySkeleton");
    }

    if (!consumer_ready_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (consumer ready) was stopped by stop_token instead of notification");
    }

    {
        const auto update_result = service.test_field.Update(kUpdatedValue);
        if (!update_result.has_value())
        {
            FailTest("Provider: Unable to update field with updated value: ", update_result.error());
        }
    }

    if (!done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (done) was stopped by stop_token instead of notification");
    }

    service.StopOfferService();
}

void run_set_and_notifier_provider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create ProcessSynchronizer");
    }

    SkeletonContainer<SetEnabledSkeleton> skeleton_container{kInstanceSpecifierString};
    if (!skeleton_container.CreateSkeleton())
    {
        FailTest("Provider: Unable to construct SetEnabledSkeleton");
    }

    auto& service = skeleton_container.GetSkeleton();
    const auto register_handler_result = service.test_field.RegisterSetHandler([](std::int32_t& value) noexcept {
        value = value * 2 + 1;
    });
    if (!register_handler_result.has_value())
    {
        FailTest("Provider: Unable to register set handler: ", register_handler_result.error());
    }

    const auto update_result = service.test_field.Update(kInitialValue);
    if (!update_result.has_value())
    {
        FailTest("Provider: Unable to update initial field value: ", update_result.error());
    }

    if (!skeleton_container.OfferService())
    {
        FailTest("Provider: Unable to offer SetEnabledSkeleton");
    }

    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort was stopped by stop_token instead of notification");
    }

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
