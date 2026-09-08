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
#include "score/mw/com/test/fields/getter_setter_if_available/consumer.h"

#include "score/mw/com/com_error_domain.h"
#include "score/mw/com/types.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/fields/getter_setter_if_available/common_resources.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/notifier_only_enabled_field.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/setter_and_getter_enabled_field.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/setter_only_enabled_field.h"
#include "score/mw/com/test/fields/test_resources/getter_and_setter_checkers.h"

#include <score/stop_token.hpp>

#include <cstdint>
#include <iostream>
#include <string>

namespace score::mw::com::test
{
namespace
{

constexpr std::int32_t kInitialValue{42};
constexpr std::int32_t kSetRequestValue{11};
constexpr std::int32_t kAcceptedSetValue{(kSetRequestValue * 2) + 1};
constexpr auto kConsumerDoneShmPath = "/score_mw_com_test_fields_getter_setter_if_available_consumer_done";

const auto kInstanceSpecifierUseGetAndSetEnabled =
    InstanceSpecifier::Create(std::string{"/score/mw/com/test/fields/getter_setter_if_available/get_true_set_true"})
        .value();
const auto kInstanceSpecifierUseGetEnabledOnly =
    InstanceSpecifier::Create(std::string{"/score/mw/com/test/fields/getter_setter_if_available/get_true_set_false"})
        .value();
const auto kInstanceSpecifierUseSetEnabledOnly =
    InstanceSpecifier::Create(std::string{"/score/mw/com/test/fields/getter_setter_if_available/get_false_set_true"})
        .value();
const auto kInstanceSpecifierUseGetAndSetDisabled =
    InstanceSpecifier::Create(std::string{"/score/mw/com/test/fields/getter_setter_if_available/get_false_set_false"})
        .value();

template <typename ProxyFieldType>
void ExpectGetDisabled(ProxyFieldType& proxy_field)
{
    const auto get_result = proxy_field.Get();
    if (get_result.has_value())
    {
        FailTest("Consumer: Get() succeeded although useGetIfAvailable is false");
    }
    if (get_result.error() != ComErrc::kMethodBindingDisabled)
    {
        FailTest("Consumer: Get() failed with unexpected error: ", get_result.error());
    }
}

template <typename ProxyFieldType>
void ExpectSetDisabled(ProxyFieldType& proxy_field, const std::int32_t requested_value)
{
    const auto set_result = proxy_field.Set(requested_value);
    if (set_result.has_value())
    {
        FailTest("Consumer: Set() succeeded although useSetIfAvailable is false");
    }
    if (set_result.error() != ComErrc::kMethodBindingDisabled)
    {
        FailTest("Consumer: Set() failed with unexpected error: ", set_result.error());
    }
}

template <typename ProxyType, TestMode mode>
void RunConsumerScenario()
{
    // Step 1. Create process synchronizer to notify provider when consumer finished
    std::cout << "\nConsumer: Step 1 - Create done synchronizer" << std::endl;
    auto consumer_done_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!consumer_done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&consumer_done_synchronizer_result]() {
        consumer_done_synchronizer_result->Notify();
    }};

    // Step 2. Create one proxy container per getter/setter availability configuration
    std::cout << "\nConsumer: Step 2 - Create proxy containers" << std::endl;
    ProxyContainer<ProxyType> proxy_use_get_and_set_enabled_container{};
    ProxyContainer<ProxyType> proxy_use_get_enabled_only_container{};
    ProxyContainer<ProxyType> proxy_use_set_enabled_only_container{};
    ProxyContainer<ProxyType> proxy_use_get_and_set_disabled_container{};

    proxy_use_get_and_set_enabled_container.CreateProxy(kInstanceSpecifierUseGetAndSetEnabled,
                                                        "proxy_use_get_and_set_enabled_container");
    proxy_use_get_enabled_only_container.CreateProxy(kInstanceSpecifierUseGetEnabledOnly,
                                                     "proxy_use_get_enabled_only_container");
    proxy_use_set_enabled_only_container.CreateProxy(kInstanceSpecifierUseSetEnabledOnly,
                                                     "proxy_use_set_enabled_only_container");
    proxy_use_get_and_set_disabled_container.CreateProxy(kInstanceSpecifierUseGetAndSetDisabled,
                                                         "proxy_use_get_and_set_disabled_container");

    auto& proxy_field_use_get_and_set_enabled = GetProxyField(proxy_use_get_and_set_enabled_container.GetProxy());
    auto& proxy_field_use_get_enabled_only = GetProxyField(proxy_use_get_enabled_only_container.GetProxy());
    auto& proxy_field_use_set_enabled_only = GetProxyField(proxy_use_set_enabled_only_container.GetProxy());
    auto& proxy_field_use_get_and_set_disabled = GetProxyField(proxy_use_get_and_set_disabled_container.GetProxy());

    if constexpr (HasGetter<mode>())
    {
        // Step 3. Verify getter behavior for enabled and disabled useGetIfAvailable combinations
        std::cout << "\nConsumer: Step 3 - Verify getter behavior" << std::endl;
        CallGetAndCheckValue(proxy_field_use_get_and_set_enabled, kInitialValue);
        CallGetAndCheckValue(proxy_field_use_get_enabled_only, kInitialValue);
        ExpectGetDisabled(proxy_field_use_set_enabled_only);
        ExpectGetDisabled(proxy_field_use_get_and_set_disabled);
    }

    if constexpr (HasSetter<mode>())
    {
        // Step 3/4. Verify setter behavior for enabled and disabled useSetIfAvailable combinations
        const auto step_idx = HasGetter<mode>() ? 4 : 3;
        std::cout << "\nConsumer: Step " << step_idx << " - Verify setter behavior" << std::endl;
        CallSetAndCheckReturnValue(proxy_field_use_get_and_set_enabled, kSetRequestValue, kAcceptedSetValue);
        ExpectSetDisabled(proxy_field_use_get_enabled_only, kSetRequestValue);
        CallSetAndCheckReturnValue(proxy_field_use_set_enabled_only, kSetRequestValue, kAcceptedSetValue);
        ExpectSetDisabled(proxy_field_use_get_and_set_disabled, kSetRequestValue);
    }
}

template <TestMode mode>
void RunConsumerForMode()
{
    if constexpr (mode == TestMode::kNotifierOnly)
    {
        RunConsumerScenario<NotifierOnlyEnabledProxy, mode>();
    }
    else if constexpr (mode == TestMode::kSetterOnly)
    {
        RunConsumerScenario<SetterOnlyEnabledProxy, mode>();
    }
    else if constexpr (mode == TestMode::kGetterOnly)
    {
        RunConsumerScenario<GetterOnlyEnabledProxy, mode>();
    }
    else if constexpr (mode == TestMode::kSetterAndGetter)
    {
        RunConsumerScenario<SetterAndGetterEnabledProxy, mode>();
    }
}

}  // namespace

void run_consumer(const TestMode mode)
{
    switch (mode)
    {
        case TestMode::kNotifierOnly:
            RunConsumerForMode<TestMode::kNotifierOnly>();
            return;
        case TestMode::kSetterOnly:
            RunConsumerForMode<TestMode::kSetterOnly>();
            return;
        case TestMode::kGetterOnly:
            RunConsumerForMode<TestMode::kGetterOnly>();
            return;
        case TestMode::kSetterAndGetter:
            RunConsumerForMode<TestMode::kSetterAndGetter>();
            return;
    }

    FailTest("Consumer: unsupported test mode");
}

}  // namespace score::mw::com::test
