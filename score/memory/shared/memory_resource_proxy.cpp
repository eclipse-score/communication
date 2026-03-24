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
#include "score/memory/shared/memory_resource_proxy.h"

#include "score/memory/shared/memory_region_bounds.h"
#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <exception>

namespace score::memory::shared
{

namespace
{

bool IsMemoryResourceProxyWithinMemoryBounds(const MemoryResourceProxy* const ptr,
                                             const MemoryRegionBounds& memory_bounds)
{
    const auto ptr_as_integer = CastPointerToInteger(static_cast<const void*>(ptr));
    return ((ptr_as_integer >= memory_bounds.GetStartAddress()) && (ptr_as_integer <= memory_bounds.GetEndAddress()));
}

}  // namespace

// per default (safety) bounds-checking is enabled.
bool MemoryResourceProxy::bounds_checking_enabled_{true};

bool MemoryResourceProxy::EnableBoundsChecking(const bool enable) noexcept
{
    auto previous_value = bounds_checking_enabled_;
    bounds_checking_enabled_ = enable;
    return previous_value;
}

MemoryResourceProxy::MemoryResourceProxy(const std::uint64_t identifier) : memory_identifier_{identifier} {}

auto MemoryResourceProxy::allocate(const std::size_t bytes, const std::size_t alignment) const noexcept -> void*
{
    if (bounds_checking_enabled_)
    {
        PerformBoundsCheck(memory_identifier_);
    }
    auto memoryResource = MemoryResourceRegistry::getInstance().at(this->memory_identifier_);
    // it is already ensured via bounds check, that the memory resource exists
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(memoryResource != nullptr);
    return memoryResource->allocate(bytes, alignment);
}

auto MemoryResourceProxy::deallocate(void* const memory, const std::size_t byte) const -> void
{
    auto memoryResource = MemoryResourceRegistry::getInstance().at(this->memory_identifier_);
    if (memoryResource != nullptr)
    {
        return memoryResource->deallocate(memory, byte);
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly."
// Rationale: score::Result value() can throw an exception if it's called without a value. Since we check has_value()
// before calling value(), an exception will never be called and therefore there will never be an implicit
// std::terminate call.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void MemoryResourceProxy::PerformBoundsCheck(const std::uint64_t memory_identifier) const noexcept
{
    auto memory_bounds =
        score::memory::shared::MemoryResourceRegistry::getInstance().GetBoundsFromIdentifier(memory_identifier);
    if (!memory_bounds.has_value())
    {
        score::mw::log::LogError("shm") << __func__ << __LINE__
                                      << "MemoryResourceProxy's memory identifier:" << memory_identifier
                                      << "could not be found in MemoryResourceRegistry";
        std::terminate();
    }

    // Check that the start address of MemoryResourceProxy lies inside the shared memory region.
    if (!IsMemoryResourceProxyWithinMemoryBounds(this, memory_bounds.value()))
    {
        score::mw::log::LogError("shm") << __func__ << __LINE__ << "MemoryResourceProxy at"
                                      << CastPointerToInteger(static_cast<const void*>(this))
                                      << "is out of memory bounds: [" << memory_bounds->GetStartAddress() << ":"
                                      << memory_bounds->GetEndAddress() << "]";
        std::terminate();
    }
}

bool operator==(const MemoryResourceProxy& lhs, const MemoryResourceProxy& rhs) noexcept
{
    return lhs.memory_identifier_ == rhs.memory_identifier_;
}

bool operator!=(const MemoryResourceProxy& lhs, const MemoryResourceProxy& rhs) noexcept
{
    return !(lhs == rhs);
}

}  // namespace score::memory::shared
