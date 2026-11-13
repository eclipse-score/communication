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
#include "score/mw/com/impl/serialization/serializer_lookup_key.h"

#include <array>
#include <cstdio>

namespace score::mw::com::impl::serialization
{

namespace
{

std::string ToPaddedHex(std::uint64_t value)
{
    std::array<char, 17U> buffer{};
    // 16 hex digits is enough to represent any std::uint64_t value.
    (void)std::snprintf(buffer.data(), buffer.size(), "%016llx", static_cast<unsigned long long>(value));
    return std::string{buffer.data()};
}

}  // namespace

SerializerLookupKey::SerializerLookupKey(std::uint64_t structural_hash, std::uint64_t attribute_hash) noexcept
    : structural_hash_{structural_hash}, attribute_hash_{attribute_hash}
{
}

std::uint64_t SerializerLookupKey::GetStructuralHash() const noexcept
{
    return structural_hash_;
}

std::uint64_t SerializerLookupKey::GetAttributeHash() const noexcept
{
    return attribute_hash_;
}

std::string SerializerLookupKey::FactorySymbolName() const
{
    return "create_serializer_" + ToPaddedHex(structural_hash_) + "_" + ToPaddedHex(attribute_hash_);
}

}  // namespace score::mw::com::impl::serialization
