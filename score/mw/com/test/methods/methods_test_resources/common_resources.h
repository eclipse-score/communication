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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_COMMON_RESOURCES_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_COMMON_RESOURCES_H
/// This file contains information shared between the provider and consumer.
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace score::mw::com::test
{

enum class CopyMode : std::uint8_t
{
    ZERO_COPY,
    WITH_COPY
};

inline constexpr std::size_t kMaxRegisteredMethos{5U};
inline constexpr std::int32_t kMethodResultMultiplierBase = 13;

inline constexpr std::string_view kInstanceSpecifierSV = "methods_test_resources/MultiMethodProvider";

auto CreateInterprocessNotificationShmPath(std::size_t path_id) -> std::string;

void InitializeRuntime(const std::string& path);

}  // namespace score::mw::com::test
#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_COMMON_RESOURCES_H
