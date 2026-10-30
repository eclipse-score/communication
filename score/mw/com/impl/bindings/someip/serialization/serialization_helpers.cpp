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
 *******************************************************************************/
#include "score/mw/com/impl/bindings/someip/serialization/serialization_helpers.h"

namespace score::mw::com::impl::someip::serialization
{

void WriteBool(const bool value, const score::cpp::span<std::uint8_t> out) noexcept
{
    out[0] = value ? std::uint8_t{0x01U} : std::uint8_t{0x00U};
}

bool ReadBool(const score::cpp::span<const std::uint8_t> in) noexcept
{
    // Only the least significant bit is evaluated; all other bits are disregarded.
    return (in[0] & std::uint8_t{0x01U}) != 0U;
}

}  // namespace score::mw::com::impl::someip::serialization
