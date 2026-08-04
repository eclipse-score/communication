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
#include "score/mw/com/test/fields/getter_setter_if_available/provider.h"

#include "score/mw/com/types.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/fields/getter_setter_if_available/common_resources.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/getter_only_enabled_field.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/notifier_only_enabled_field.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/setter_and_getter_enabled_field.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/setter_only_enabled_field.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace score::mw::com::test
{
namespace
{

constexpr std::int32_t kInitialValue{42};
constexpr auto kConsumerDoneShmPath = "/score_mw_com_test_fields_getter_setter_if_available_consumer_done";

const auto kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"/score/mw/com/test/fields/getter_setter_if_available/provider"}).value();

template <typename SkeletonType, TestMode mode>
void RunProviderScenario(const score::cpp::stop_token& stop_token)
{
    // Step 1. Create process synchronizer to wait for consumer completion notification
    std::cout << "\nProvider: Step 1 - Create done synchronizer" << std::endl;
    auto done_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Provider: Could not create ProcessSynchronizer");
    }

    // Step 2. Create skeleton and get field under test
    std::cout << "\nProvider: Step 2 - Create skeleton and access field" << std::endl;
    SkeletonContainer<SkeletonType> skeleton_container{};

    skeleton_container.CreateSkeleton(kInstanceSpecifier, "provider_skeleton_container");
    auto& skeleton = skeleton_container.GetSkeleton();
    auto& field = GetSkeletonField(skeleton);

    // Step 3. Register set handler for modes where the field provides Set()
    std::cout << "\nProvider: Step 3 - Register set handler when available" << std::endl;
    if constexpr (HasSetter<mode>())
    {
        const auto register_result = field.RegisterSetHandler([](std::int32_t& value) noexcept {
            value = (value * 2) + 1;
        });
        if (!register_result.has_value())
        {
            FailTest("Provider: Unable to register set handler: ", register_result.error());
        }
    }

    // Step 4. Update initial field value
    std::cout << "\nProvider: Step 4 - Update initial field value" << std::endl;
    const auto update_result = field.Update(kInitialValue);
    if (!update_result.has_value())
    {
        FailTest("Provider: Unable to update initial field value: ", update_result.error());
    }

    // Step 5. Offer the service so consumers can create proxies and execute checks
    std::cout << "\nProvider: Step 5 - Offer service" << std::endl;
    skeleton_container.OfferService("getter_setter_if_available_provider");

    // Step 6. Wait for consumer completion notification
    std::cout << "\nProvider: Step 6 - Wait for consumer completion" << std::endl;
    if (!done_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort was stopped by stop_token instead of notification");
    }
}

template <TestMode mode>
void RunProviderForMode(const score::cpp::stop_token& stop_token)
{
    if constexpr (mode == TestMode::kNotifierOnly)
    {
        RunProviderScenario<NotifierOnlyEnabledSkeleton, mode>(stop_token);
    }
    else if constexpr (mode == TestMode::kSetterOnly)
    {
        RunProviderScenario<SetterOnlyEnabledSkeleton, mode>(stop_token);
    }
    else if constexpr (mode == TestMode::kGetterOnly)
    {
        RunProviderScenario<GetterOnlyEnabledSkeleton, mode>(stop_token);
    }
    else if constexpr (mode == TestMode::kSetterAndGetter)
    {
        RunProviderScenario<SetterAndGetterEnabledSkeleton, mode>(stop_token);
    }
}

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token, const TestMode mode)
{
    switch (mode)
    {
        case TestMode::kNotifierOnly:
            RunProviderForMode<TestMode::kNotifierOnly>(stop_token);
            return;
        case TestMode::kSetterOnly:
            RunProviderForMode<TestMode::kSetterOnly>(stop_token);
            return;
        case TestMode::kGetterOnly:
            RunProviderForMode<TestMode::kGetterOnly>(stop_token);
            return;
        case TestMode::kSetterAndGetter:
            RunProviderForMode<TestMode::kSetterAndGetter>(stop_token);
            return;
    }

    FailTest("Provider: unsupported test mode");
}

}  // namespace score::mw::com::test
