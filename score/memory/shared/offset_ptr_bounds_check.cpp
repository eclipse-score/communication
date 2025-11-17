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

#include <cstddef>

namespace score::memory::shared
{

namespace
{

/// \brief Checks, whether pointer points to an address within given bounds.
/// \attention The end-bound is non-inclusive! \see MemoryResourceRegistry::GetBoundsFromAddress
/// \param ptr pointer target
/// \param memory_bounds
/// \return true in case the pointed to address is within the bounds.
bool IsPointerWithinMemoryBounds(const std::uintptr_t ptr_as_integer, const MemoryRegionBounds& memory_bounds)
{
    return ((ptr_as_integer >= memory_bounds.GetStartAddress()) && (ptr_as_integer <= memory_bounds.GetEndAddress()));
}

}  // namespace

bool DoesOffsetPtrInSharedMemoryPassBoundsChecks(const void* const offset_ptr_address,
                                                 const std::ptrdiff_t offset,
                                                 const MemoryRegionBounds& offset_ptr_memory_bounds,
                                                 const std::size_t pointed_type_size,
                                                 const std::size_t offset_ptr_size) noexcept
{
    // Check that the entire OffsetPtr lies inside the shared memory region.
    const auto offset_ptr_address_as_integer = CastPointerToInteger(offset_ptr_address);
    const auto offset_ptr_end_address_as_integer =
        AddOffsetToPointerAsInteger(offset_ptr_address_as_integer, offset_ptr_size);
    if (!IsPointerWithinMemoryBounds(offset_ptr_end_address_as_integer, offset_ptr_memory_bounds))
    {
        ::score::mw::log::LogError("shm") << __func__ << __LINE__ << "OffsetPtr at"
                                        << CastPointerToInteger(offset_ptr_address)
                                        << "does not fit completely in memory region: ["
                                        << offset_ptr_memory_bounds.GetStartAddress() << ":"
                                        << offset_ptr_memory_bounds.GetEndAddress() << "]";
        return false;
    }

    // Check that the start address of the pointed-to object lies inside the shared memory region.
    const auto pointed_to_start_address_as_integer =
        AddSignedOffsetToPointerAsInteger(offset_ptr_address_as_integer, offset);
    if (!IsPointerWithinMemoryBounds(pointed_to_start_address_as_integer, offset_ptr_memory_bounds))
    {
        ::score::mw::log::LogError("shm") << __func__ << __LINE__ << "OffsetPtr at"
                                        << CastPointerToInteger(offset_ptr_address) << "is pointing to address "
                                        << pointed_to_start_address_as_integer
                                        << "which lies outside the OffsetPtr's memory region: ["
                                        << offset_ptr_memory_bounds.GetStartAddress() << ":"
                                        << offset_ptr_memory_bounds.GetEndAddress() << "]";
        return false;
    }

    // Check that the end address of the pointed-to object lies inside the shared memory region.
    const auto pointed_to_end_address_as_integer =
        AddOffsetToPointerAsInteger(pointed_to_start_address_as_integer, pointed_type_size);
    if (!IsPointerWithinMemoryBounds(pointed_to_end_address_as_integer, offset_ptr_memory_bounds))
    {
        ::score::mw::log::LogError("shm") << __func__ << __LINE__ << "OffsetPtr at"
                                        << CastPointerToInteger(offset_ptr_address) << "is pointing to address "
                                        << pointed_to_end_address_as_integer
                                        << "which does not fit completely within the OffsetPtr's memory region: ["
                                        << offset_ptr_memory_bounds.GetStartAddress() << ":"
                                        << offset_ptr_memory_bounds.GetEndAddress() << "]";
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
bool DoesOffsetPtrNotInSharedMemoryPassBoundsChecks(const void* const offset_ptr_address,
                                                    const std::ptrdiff_t offset,
                                                    const MemoryRegionBounds& offset_ptr_memory_bounds,
                                                    const std::size_t pointed_type_size,
                                                    const std::size_t offset_ptr_size) noexcept
{
    // Check that the entire OffsetPtr lies outside a memory region
    const auto offset_ptr_address_as_integer = CastPointerToInteger(offset_ptr_address);
    const auto offset_ptr_end_address_as_integer =
        AddOffsetToPointerAsInteger(offset_ptr_address_as_integer, offset_ptr_size);
    const auto offset_ptr_end_address_bounds =
        MemoryResourceRegistry::getInstance().GetBoundsFromAddressAsInteger(offset_ptr_end_address_as_integer);
    if (offset_ptr_end_address_bounds.has_value())
    {
        ::score::mw::log::LogError("shm") << __func__ << __LINE__ << "OffsetPtr at"
                                        << CastPointerToInteger(offset_ptr_address)
                                        << "is overlapping the start of memory region: ["
                                        << offset_ptr_end_address_bounds.value().GetStartAddress() << ":"
                                        << offset_ptr_end_address_bounds.value().GetEndAddress() << "]";
        return false;
    }

    // If the OffsetPtr is not within a memory resource, we check if it contains valid memory bounds
    // which indicates that it was previously in a shared memory region and was copied out.
    if (offset_ptr_memory_bounds.has_value())
    {
        // Check that the start address of the pointed-to object lies inside the shared memory region.
        const auto pointed_to_start_address_as_integer =
            AddSignedOffsetToPointerAsInteger(offset_ptr_address_as_integer, offset);
        if (!IsPointerWithinMemoryBounds(pointed_to_start_address_as_integer, offset_ptr_memory_bounds))
        {
            ::score::mw::log::LogError("shm")
                << __func__ << __LINE__ << "OffsetPtr at" << CastPointerToInteger(offset_ptr_address)
                << "is pointing to address " << pointed_to_start_address_as_integer
                << "which lies outside the OffsetPtr's memory region: [" << offset_ptr_memory_bounds.GetStartAddress()
                << ":" << offset_ptr_memory_bounds.GetEndAddress() << "]";
            return false;
        }

        // Check that the end address of the pointed-to object lies inside the shared memory region.
        const auto pointed_to_end_address_as_integer =
            AddOffsetToPointerAsInteger(pointed_to_start_address_as_integer, pointed_type_size);
        if (!IsPointerWithinMemoryBounds(pointed_to_end_address_as_integer, offset_ptr_memory_bounds))
        {
            ::score::mw::log::LogError("shm")
                << __func__ << __LINE__ << "OffsetPtr at" << CastPointerToInteger(offset_ptr_address)
                << "is pointing to address " << pointed_to_end_address_as_integer
                << "which does not fit completely within the OffsetPtr's memory region: ["
                << offset_ptr_memory_bounds.GetStartAddress() << ":" << offset_ptr_memory_bounds.GetEndAddress() << "]";
            return false;
        }
    }
    return true;
}

}  // namespace score::memory::shared
