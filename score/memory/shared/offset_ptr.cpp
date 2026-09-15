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
#include "score/memory/shared/offset_ptr.h"

namespace score::memory::shared
{

namespace
{

/// \brief global (process wide) flag, whether bounds-checking shall be done.
/// \details defaults to true (for safety reasons). Users of shared-memory/OffsetPtr infrastructure can enable/disable
///          it via EnableOffsetPtrBoundsChecking(bool enable)
bool bounds_checking_enabled = true;

}  // anonymous namespace

namespace detail_offset_ptr
{

bool IsBoundsCheckingEnabled() noexcept
{
    return bounds_checking_enabled;
}

void AssertOffsetBoundsCheck(const void* const address,
                             const difference_type offset,
                             const MemoryRegionBounds& memory_bounds_when_not_in_shm,
                             const std::size_t pointed_type_size,
                             const std::size_t own_size)
{
    if (IsBoundsCheckingEnabled())
    {
        const auto bounds = MemoryResourceRegistry::getInstance().GetBoundsFromAddress(address);
        const auto is_in_memory_region = bounds.has_value();
        if (is_in_memory_region)
        {
            // We use SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD instead of std::terminate so that we can check these in unit
            // tests using SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED instead of death tests (since death tests
            // are very slow).
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(DoesOffsetPtrInSharedMemoryPassBoundsChecks(
                address, offset, bounds.value(), pointed_type_size, own_size));
        }
        else
        {
            // We use SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD instead of std::terminate so that we can check these in unit
            // tests using SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED instead of death tests (since death tests
            // are very slow).
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(DoesOffsetPtrNotInSharedMemoryPassBoundsChecks(
                address, offset, memory_bounds_when_not_in_shm, pointed_type_size, own_size));
        }
    }
}

}  // namespace detail_offset_ptr

bool EnableOffsetPtrBoundsChecking(const bool enable)
{
    const bool previous_value = bounds_checking_enabled;
    bounds_checking_enabled = enable;
    return previous_value;
}

}  // namespace score::memory::shared
