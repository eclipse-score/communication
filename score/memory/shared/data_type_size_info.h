/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_UTIL_DATA_TYPE_SIZE_INFO_H
#define SCORE_MW_COM_IMPL_UTIL_DATA_TYPE_SIZE_INFO_H

#include <cstddef>

namespace score::memory::shared
{

struct DataTypeSizeInfo
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types
    // shall be private.". This struct is a POD class!
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::size_t size{};
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::size_t alignment{};
};

bool operator==(const DataTypeSizeInfo& lhs, const DataTypeSizeInfo& rhs) noexcept;

}  // namespace score::memory::shared

#endif  // SCORE_MW_COM_IMPL_UTIL_DATA_TYPE_SIZE_INFO_H
