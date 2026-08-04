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
#ifndef SCORE_MW_COM_TEST_FIELDS_GETTER_SETTER_IF_AVAILABLE_COMMON_RESOURCES_H
#define SCORE_MW_COM_TEST_FIELDS_GETTER_SETTER_IF_AVAILABLE_COMMON_RESOURCES_H

#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/getter_only_enabled_field.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/notifier_only_enabled_field.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/setter_and_getter_enabled_field.h"
#include "score/mw/com/test/fields/getter_setter_if_available/datatypes/setter_only_enabled_field.h"
#include "score/mw/com/types.h"

#include <string>

namespace score::mw::com::test
{

const auto kInstanceSpecifierUseGetAndSetEnabled =
    InstanceSpecifier::Create(
        std::string{"/score/mw/com/test/fields/getter_setter_if_available/use_get_and_set_enabled"})
        .value();
const auto kInstanceSpecifierUseGetEnabledOnly =
    InstanceSpecifier::Create(std::string{"/score/mw/com/test/fields/getter_setter_if_available/use_get_enabled_only"})
        .value();
const auto kInstanceSpecifierUseSetEnabledOnly =
    InstanceSpecifier::Create(std::string{"/score/mw/com/test/fields/getter_setter_if_available/use_set_enabled_only"})
        .value();
const auto kInstanceSpecifierUseGetAndSetDisabled =
    InstanceSpecifier::Create(
        std::string{"/score/mw/com/test/fields/getter_setter_if_available/use_get_and_set_disabled"})
        .value();

enum class TestMode
{
    kNotifierOnly,
    kSetterOnly,
    kGetterOnly,
    kSetterAndGetter,
};

struct TestConfig
{
    TestMode mode;
    std::string config_file_path;
};

TestConfig ParseConfig(int argc, const char** argv);

template <TestMode mode>
constexpr bool HasGetter()
{
    return (mode == TestMode::kGetterOnly || mode == TestMode::kSetterAndGetter);
}

template <TestMode mode>
constexpr bool HasSetter()
{
    return (mode == TestMode::kSetterOnly || mode == TestMode::kSetterAndGetter);
}

inline auto& GetProxyField(NotifierOnlyEnabledProxy& proxy)
{
    return proxy.notifier_only_enabled_field;
}

inline auto& GetProxyField(SetterOnlyEnabledProxy& proxy)
{
    return proxy.setter_only_enabled_field;
}

inline auto& GetProxyField(GetterOnlyEnabledProxy& proxy)
{
    return proxy.getter_only_enabled_field;
}

inline auto& GetProxyField(SetterAndGetterEnabledProxy& proxy)
{
    return proxy.setter_and_getter_enabled_field;
}

inline auto& GetSkeletonField(NotifierOnlyEnabledSkeleton& skeleton)
{
    return skeleton.notifier_only_enabled_field;
}

inline auto& GetSkeletonField(SetterOnlyEnabledSkeleton& skeleton)
{
    return skeleton.setter_only_enabled_field;
}

inline auto& GetSkeletonField(GetterOnlyEnabledSkeleton& skeleton)
{
    return skeleton.getter_only_enabled_field;
}

inline auto& GetSkeletonField(SetterAndGetterEnabledSkeleton& skeleton)
{
    return skeleton.setter_and_getter_enabled_field;
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_GETTER_SETTER_IF_AVAILABLE_COMMON_RESOURCES_H
