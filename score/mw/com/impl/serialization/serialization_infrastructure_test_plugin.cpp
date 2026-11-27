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

#include <cstdint>

namespace
{

constexpr std::uint32_t kBackendProbeValue = 0xC0FFEE42U;

}  // namespace

extern "C" {

std::uint32_t BackendProbeValue() noexcept
{
    return kBackendProbeValue;
}

}  // extern "C"
