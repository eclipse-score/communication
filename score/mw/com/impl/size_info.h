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
#pragma once

#include <cstddef>

namespace score::mw::com
{
/// @brief A struct to hold size and alignment information for generic type-erased data.
struct SizeInfo
{
    size_t size;
    size_t alignment;
};

}  // namespace score::mw::com