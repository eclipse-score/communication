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
#include "score/memory/shared/memory_region_bounds.h"

#include <score/assert.hpp>

namespace score::memory::shared
{

namespace
{

constexpr std::uintptr_t kInvalidAddress{0U};

bool AreBothValidOrBothInvalid(const std::uintptr_t start_address, const std::uintptr_t end_address) noexcept
{
    const bool both_valid = ((start_address == kInvalidAddress) && (end_address == kInvalidAddress));
    const bool both_invalid = ((start_address != kInvalidAddress) && (end_address != kInvalidAddress));
    return both_valid || both_invalid;
}

}  // namespace

MemoryRegionBounds::MemoryRegionBounds() noexcept : MemoryRegionBounds{kInvalidAddress, kInvalidAddress} {}

MemoryRegionBounds::MemoryRegionBounds(const std::uintptr_t start_address, const std::uintptr_t end_address) noexcept
    : start_address_{start_address}, end_address_{end_address}
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(AreBothValidOrBothInvalid(start_address, end_address));
}

void MemoryRegionBounds::Set(const std::uintptr_t start_address, const std::uintptr_t end_address) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(AreBothValidOrBothInvalid(start_address, end_address));
    start_address_ = start_address;
    end_address_ = end_address;
}

void MemoryRegionBounds::Reset() noexcept
{
    Set(kInvalidAddress, kInvalidAddress);
}

bool MemoryRegionBounds::has_value() const noexcept
{
    return !((start_address_ == kInvalidAddress) || (end_address_ == kInvalidAddress));
}

std::uintptr_t MemoryRegionBounds::GetStartAddress() const noexcept
{
    return start_address_;
}

std::uintptr_t MemoryRegionBounds::GetEndAddress() const noexcept
{
    return end_address_;
}

bool operator==(const MemoryRegionBounds& lhs, const MemoryRegionBounds& rhs) noexcept
{
    return ((lhs.GetStartAddress() == rhs.GetStartAddress()) && (lhs.GetEndAddress() == rhs.GetEndAddress()));
}

bool operator!=(const MemoryRegionBounds& lhs, const MemoryRegionBounds& rhs) noexcept
{
    return !(lhs == rhs);
}

}  // namespace score::memory::shared
