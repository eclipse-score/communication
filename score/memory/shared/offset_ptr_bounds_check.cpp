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
#include "score/memory/shared/offset_ptr_bounds_check.h"

#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include "score/mw/log/logging.h"

namespace score::memory::shared
{

namespace
{

/// \brief Checks, whether pointer points to an address within given bounds.
/// \attention The end-bound is non-inclusive! \see MemoryResourceRegistry::GetBoundsFromAddress
/// \param ptr pointer target
/// \param memory_bounds
/// \return true in case the pointed to address is within the bounds.
bool IsPointerWithinMemoryBounds(const void* const ptr, const MemoryResourceRegistry::MemoryBounds& memory_bounds)
{
    const auto ptr_as_integer = CastPointerToInteger(ptr);
    return ((ptr_as_integer >= memory_bounds.first) && (ptr_as_integer <= memory_bounds.second));
}

}  // namespace

bool DoesOffsetPtrInSharedMemoryPassBoundsChecks(const void* const offset_ptr_address,
                                                 const std::ptrdiff_t offset,
                                                 const MemoryResourceRegistry::MemoryBounds& offset_ptr_memory_bounds,
                                                 const std::size_t pointed_type_size,
                                                 const std::size_t offset_ptr_size) noexcept
{
    // Check that the entire OffsetPtr lies inside the shared memory region.
    const auto* const offset_ptr_end_address = AddOffsetToPointer(offset_ptr_address, offset_ptr_size);
    if (!IsPointerWithinMemoryBounds(offset_ptr_end_address, offset_ptr_memory_bounds))
    {
        ::score::mw::log::LogError("shm") << __func__ << __LINE__ << "OffsetPtr at"
                                        << CastPointerToInteger(offset_ptr_address)
                                        << "does not fit completely in memory region: ["
                                        << offset_ptr_memory_bounds.first << ":" << offset_ptr_memory_bounds.second
                                        << "]";
        return false;
    }

    // Check that the start address of the pointed-to object lies inside the shared memory region.
    const auto* const pointed_to_start_address = AddOffsetToPointer(offset_ptr_address, offset);
    if (!IsPointerWithinMemoryBounds(pointed_to_start_address, offset_ptr_memory_bounds))
    {
        ::score::mw::log::LogError("shm") << __func__ << __LINE__ << "OffsetPtr at"
                                        << CastPointerToInteger(offset_ptr_address) << "is pointing to address "
                                        << CastPointerToInteger(pointed_to_start_address)
                                        << "which lies outside the OffsetPtr's memory region: ["
                                        << offset_ptr_memory_bounds.first << ":" << offset_ptr_memory_bounds.second
                                        << "]";
        return false;
    }

    // Check that the end address of the pointed-to object lies inside the shared memory region.
    const auto* const pointed_to_end_address = AddOffsetToPointer(pointed_to_start_address, pointed_type_size);
    if (!IsPointerWithinMemoryBounds(pointed_to_end_address, offset_ptr_memory_bounds))
    {
        ::score::mw::log::LogError("shm") << __func__ << __LINE__ << "OffsetPtr at"
                                        << CastPointerToInteger(offset_ptr_address) << "is pointing to address "
                                        << CastPointerToInteger(pointed_to_end_address)
                                        << "which does not fit completely within the OffsetPtr's memory region: ["
                                        << offset_ptr_memory_bounds.first << ":" << offset_ptr_memory_bounds.second
                                        << "]";
        return false;
    }
    return true;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly."
// Rationale: score::Result value() can throw an exception if it's called without a value. Since we check has_value()
// before calling value(), an exception will never be called and therefore there will never be an implicit
// std::terminate call.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
bool DoesOffsetPtrNotInSharedMemoryPassBoundsChecks(
    const void* const offset_ptr_address,
    const std::ptrdiff_t offset,
    const MemoryResourceRegistry::MemoryResourceIdentifier memory_resource_identifier,
    const std::size_t pointed_type_size,
    const std::size_t offset_ptr_size) noexcept
{
    // Check that the entire OffsetPtr lies outside a memory region
    const auto* const offset_ptr_end_address = AddOffsetToPointer(offset_ptr_address, offset_ptr_size);
    const auto offset_ptr_end_address_bounds =
        MemoryResourceRegistry::getInstance().GetBoundsFromAddress(offset_ptr_end_address);
    if (offset_ptr_end_address_bounds.has_value())
    {
        ::score::mw::log::LogError("shm") << __func__ << __LINE__ << "OffsetPtr at"
                                        << CastPointerToInteger(offset_ptr_address)
                                        << "is overlapping the start of memory region: ["
                                        << offset_ptr_end_address_bounds.value().first.first << ":"
                                        << offset_ptr_end_address_bounds.value().first.second << "]";
        return false;
    }

    // If the OffsetPtr is not within a memory resource, we check if it contains a memory_resource_identifier_
    // which indicates that it was previosuly in a shared memory region and was copied out.
    if (memory_resource_identifier != 0U)
    {
        const auto offset_ptr_memory_bounds_from_identifier =
            MemoryResourceRegistry::getInstance().GetBoundsFromIdentifier(memory_resource_identifier);
        const auto& offset_ptr_memory_bounds = offset_ptr_memory_bounds_from_identifier.value();

        // Check that the start address of the pointed-to object lies inside the shared memory region.
        const auto* const pointed_to_start_address = AddOffsetToPointer(offset_ptr_address, offset);
        if (!IsPointerWithinMemoryBounds(pointed_to_start_address, offset_ptr_memory_bounds))
        {
            ::score::mw::log::LogError("shm")
                << __func__ << __LINE__ << "OffsetPtr at" << CastPointerToInteger(offset_ptr_address)
                << "is pointing to address " << CastPointerToInteger(pointed_to_start_address)
                << "which lies outside the OffsetPtr's memory region: [" << offset_ptr_memory_bounds.first << ":"
                << offset_ptr_memory_bounds.second << "]";
            return false;
        }

        // Check that the end address of the pointed-to object lies inside the shared memory region.
        const auto* const pointed_to_end_address = AddOffsetToPointer(pointed_to_start_address, pointed_type_size);
        if (!IsPointerWithinMemoryBounds(pointed_to_end_address, offset_ptr_memory_bounds))
        {
            ::score::mw::log::LogError("shm")
                << __func__ << __LINE__ << "OffsetPtr at" << CastPointerToInteger(offset_ptr_address)
                << "is pointing to address " << CastPointerToInteger(pointed_to_end_address)
                << "which does not fit completely within the OffsetPtr's memory region: ["
                << offset_ptr_memory_bounds.first << ":" << offset_ptr_memory_bounds.second << "]";
            return false;
        }
    }
    return true;
}

}  // namespace score::memory::shared
