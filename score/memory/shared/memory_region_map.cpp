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
#include "score/memory/shared/memory_region_map.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <thread>
#include <utility>

namespace score::memory::shared::detail
{

namespace
{

bool DoesRegionIteratorOverlapWithExistingRegionInMap(
    const std::map<std::uintptr_t, std::uintptr_t>::const_iterator region_it,
    const std::map<std::uintptr_t, std::uintptr_t>& map) noexcept
{
    if (map.size() == 1U)
    {
        return false;
    }

    // If region isn't the first region (i.e. region with the smallest start / end addresses) in the map, ensure that
    // the start of the current region is larger than the end of the previous region.
    if (region_it != map.cbegin())
    {
        auto previous_region_it = std::prev(region_it);

        const auto previous_region_end_address = previous_region_it->second;
        const auto current_region_start_address = region_it->first;

        if (current_region_start_address < previous_region_end_address)
        {
            return true;
        }
    }

    // If region isn't the last region (i.e. region with the largest start / end addresses) in the map, ensure that the
    // end of the current region is smaller than the start of the next region.
    auto next_region_it = std::next(region_it);
    if (next_region_it != map.cend())
    {
        const auto next_region_start_address = next_region_it->first;
        const auto current_region_end_address = region_it->second;

        if (current_region_end_address > next_region_start_address)
        {
            return true;
        }
    }
    return false;
}

}  // namespace

template <template <class> class AtomicIndirectorType>
MemoryRegionMapImpl<AtomicIndirectorType>::AcquiredRefcountIndex::AcquiredRefcountIndex(
    const std::uint8_t refcount_index,
    std::atomic<RegionVersionRefCountType>& ref_count) noexcept
    : index_{refcount_index}, ref_count_{ref_count}, owns_resource_{true}
{
}

template <template <class> class AtomicIndirectorType>
MemoryRegionMapImpl<AtomicIndirectorType>::AcquiredRefcountIndex::AcquiredRefcountIndex(
    AcquiredRefcountIndex&& other) noexcept
    // Suppress "AUTOSAR C++14 A12-8-4", The rule states: "Move constructor shall not initialize its class
    // members and base classes using copy semantics".
    // Rationale: All members are already initialized with move semantics.
    // coverity[autosar_cpp14_a12_8_4_violation : FALSE]
    : index_{std::move(other.index_)}, ref_count_{std::move(other.ref_count_)}, owns_resource_{other.owns_resource_}
{
    other.owns_resource_ = false;
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-1" rule finding. This rule states: "All user-provided class destructors, deallocation
// functions, move constructors, move assignment operators and swap functions shall not exit with an exception. A
// noexcept exception specification shall be added to these functions as appropriate."
// Rationale: False positive: An exception can never escape this function, regardless of the body, because the function
// is marked noexcept.
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly."
// Rationale: score::Result value() can throw an exception if it's called without a value. Since we check has_value()
// before calling value(), an exception will never be called and therefore there will never be an implicit
// std::terminate call.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
// coverity[autosar_cpp14_a15_5_1_violation : FALSE]
MemoryRegionMapImpl<AtomicIndirectorType>::AcquiredRefcountIndex::~AcquiredRefcountIndex() noexcept
{
    if (owns_resource_)
    {
        score::cpp::ignore = ref_count_.get().fetch_sub(1U, std::memory_order_release);
    }
}

template <template <class> class AtomicIndirectorType>
MemoryRegionMapImpl<AtomicIndirectorType>::MemoryRegionMapImpl() noexcept
{
    for (auto& element : known_regions_versions_refcounts_)
    {
        element.store(INITIAL_REF_COUNT_VALUE, std::memory_order_relaxed);
    }
    known_regions_versions_refcounts_[0] = 0U;
}

template <template <class> class AtomicIndirectorType>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly."
// Rationale: std::array::at() will throw an exception if the provided index is outside the bounds of
// the array (which will implicitly terminate in our case). We explicitly use the at() method and expect that the
// process will terminate if an invalid index is provided and do not rely on any stack unwinding in case of an implicit
// terminate.
// coverity[autosar_cpp14_a15_5_3_violation]
bool MemoryRegionMapImpl<AtomicIndirectorType>::UpdateKnownRegion(const std::uintptr_t memory_range_start,
                                                                  const std::uintptr_t memory_range_end) noexcept
{
    // acquire a version slot, which is unused to copy the current (latest) version into it
    auto new_version_idx = AcquireRegionVersionForOverwrite();
    if (new_version_idx.has_value())
    {
        //  copy latest known regions into our new version
        auto& new_known_regions = known_regions_versions_.at(static_cast<std::size_t>(new_version_idx.value()));
        new_known_regions = known_regions_versions_.at(static_cast<std::size_t>(latest_known_region_version_));
        const auto insertion_result = new_known_regions.insert({memory_range_start, memory_range_end});
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(insertion_result.second);

        // Check that the inserted range does not overlap with the previous memory region in the ordered map
        const auto inserted_region_it = insertion_result.first;
        if (DoesRegionIteratorOverlapWithExistingRegionInMap(inserted_region_it, new_known_regions))
        {
            return false;
        }

        // Set the refcount of our new version to 0
        auto& known_region_version_refcount =
            known_regions_versions_refcounts_.at(static_cast<std::size_t>(new_version_idx.value()));
        known_region_version_refcount.store(0U, std::memory_order_release);

        // ... and set the latest version to our new version
        latest_known_region_version_.store(new_version_idx.value(), std::memory_order_release);
        return true;
    }
    else
    {
        mw::log::LogFatal("shm") << "Couldn't acquire free region version for writing! Configured VERSION_COUNT ("
                                 << VERSION_COUNT << ") might be too small";
        std::terminate();
    }
}

template <template <class> class AtomicIndirectorType>
// coverity[autosar_cpp14_a15_5_3_violation] See rationale for std::array:at() autosar_cpp14_a15_5_3_violation above
void MemoryRegionMapImpl<AtomicIndirectorType>::RemoveKnownRegion(const std::uintptr_t memory_range_start) noexcept
{
    // acquire a version slot, which is unused to copy the current (latest) version into it
    auto new_version_idx = AcquireRegionVersionForOverwrite();
    if (new_version_idx.has_value())
    {
        // copy latest known regions into our new version
        auto& new_known_regions = known_regions_versions_.at(static_cast<std::size_t>(new_version_idx.value()));
        new_known_regions = known_regions_versions_.at(static_cast<std::size_t>(latest_known_region_version_));
        const auto region_it = new_known_regions.find(memory_range_start);

        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(region_it != new_known_regions.cend(),
                               "Cannot remove memory range whose start address does not exist in map.");
        score::cpp::ignore = new_known_regions.erase(region_it);

        // Set the refcount of our new version to 0
        auto& known_region_version_refcount =
            known_regions_versions_refcounts_.at(static_cast<std::size_t>(new_version_idx.value()));
        known_region_version_refcount.store(0U, std::memory_order_release);

        // ... and set the latest version to our new version
        latest_known_region_version_.store(new_version_idx.value(), std::memory_order_release);
    }
    else
    {
        mw::log::LogFatal("shm") << "Couldn't acquire free region version for writing! Configured VERSION_COUNT ("
                                 << VERSION_COUNT << ") might be too small";
        std::terminate();
    }
}

template <template <class> class AtomicIndirectorType>
// coverity[autosar_cpp14_a15_5_3_violation] See rationale for std::array:at() autosar_cpp14_a15_5_3_violation above
void MemoryRegionMapImpl<AtomicIndirectorType>::ClearKnownRegions() noexcept
{
    auto new_version_idx = AcquireRegionVersionForOverwrite();
    if (new_version_idx.has_value())
    {
        // clear the map in the new version
        known_regions_versions_.at(static_cast<std::size_t>(new_version_idx.value())).clear();

        // Set the refcount of our new version to 0
        auto& known_region_version_refcount =
            known_regions_versions_refcounts_.at(static_cast<std::size_t>(new_version_idx.value()));
        known_region_version_refcount.store(0U, std::memory_order_release);

        // ... and set the latest version to our new version
        latest_known_region_version_.store(new_version_idx.value(), std::memory_order_release);
    }
    else
    {
        mw::log::LogFatal("shm") << "Couldn't acquire free region version for writing! Configured VERSION_COUNT ("
                                 << VERSION_COUNT << ") might be too small";
        std::terminate();
    }
}

template <template <class> class AtomicIndirectorType>
// coverity[autosar_cpp14_a15_5_3_violation] See rationale for std::array:at() autosar_cpp14_a15_5_3_violation above
auto MemoryRegionMapImpl<AtomicIndirectorType>::GetBoundsFromAddress(const std::uintptr_t pointer) const noexcept
    -> std::optional<MemoryRegionBounds>
{
    auto latest_regions_version_index = AcquireLatestRegionVersionForRead();

    if (!latest_regions_version_index.has_value())
    {
        mw::log::LogFatal("shm")
            << "Couldn't acquire latest region version for reading! Unexpected refcount overflow!?";
        std::terminate();
    }

    auto& latest_known_regions =
        known_regions_versions_.at(static_cast<std::size_t>(latest_regions_version_index.value().GetIndex()));

    if (latest_known_regions.empty())
    {
        return {};
    }

    // Find first memory range which has a starting address greater than the input pointer. This will return an iterator
    // to the memory range one past the desired range.
    auto it = latest_known_regions.upper_bound(pointer);
    if (it == latest_known_regions.begin())  // Pointer address is before the first memory range
    {
        return {};
    }
    --it;

    const auto start_address = it->first;
    const auto end_address = it->second;

    const bool pointer_in_memory_bounds = ((pointer >= start_address) && (pointer <= end_address));
    if (pointer_in_memory_bounds)
    {
        const MemoryRegionBounds memory_bounds{start_address, end_address};
        return memory_bounds;
    }
    else
    {
        return {};
    }
}

template <template <class> class AtomicIndirectorType>
// coverity[autosar_cpp14_a15_5_3_violation] See rationale for std::array:at() autosar_cpp14_a15_5_3_violation above
size_t MemoryRegionMapImpl<AtomicIndirectorType>::GetSize() const noexcept
{
    auto latest_regions_version_index = AcquireLatestRegionVersionForRead();
    if (!latest_regions_version_index.has_value())
    {
        mw::log::LogFatal("shm")
            << "Couldn't acquire latest region version for reading! Unexpected refcount overflow!?";
        std::terminate();
    }

    auto& latest_known_regions =
        known_regions_versions_.at(static_cast<std::size_t>(latest_regions_version_index.value().GetIndex()));
    return latest_known_regions.size();
}

template <template <class> class AtomicIndirectorType>
// coverity[autosar_cpp14_a15_5_3_violation] See rationale for std::array:at() autosar_cpp14_a15_5_3_violation above
auto MemoryRegionMapImpl<AtomicIndirectorType>::AcquireLatestRegionVersionForRead() const noexcept
    -> std::optional<typename MemoryRegionMapImpl<AtomicIndirectorType>::AcquiredRefcountIndex>
{
    // We use an unrealistic high max_retries value here, which should never be reached.
    // It would actually need an insane number of threads, which concurrently try to increment the refcount!
    // We are using this bounded number here instead of a while(true) construct as we are then able to return
    // std::nullopt and leave it to the layer above to react e.g. with a std::terminate()!
    constexpr std::uint8_t max_retries = 255U;
    for (std::uint8_t retry_count = 0U; retry_count < max_retries; retry_count++)
    {
        const uint8_t region_index = latest_known_region_version_.load(std::memory_order_relaxed);
        const std::uint32_t previous_refcount = AtomicIndirectorType<RegionVersionRefCountType>::fetch_add(
            known_regions_versions_refcounts_.at(static_cast<size_t>(region_index)), 1U, std::memory_order_acq_rel);

        const bool num_of_concurrent_readers_overflow = (previous_refcount == (INVALID_REF_COUNT_INTERVAL_START - 1U));
        const bool num_of_reads_during_writing_overflow = (previous_refcount == INVALID_REF_COUNT_INTERVAL_END);
        if (num_of_concurrent_readers_overflow || num_of_reads_during_writing_overflow)
        {
            // We have an overflow of reader ref-count increments here, which makes no sense as it
            // means, that we have roughly 2*10⁷ refcount increments happening at a regions version ...!
            mw::log::LogFatal("shm") << "AcquireLatestRegionVersionForRead - Unexpected refcount overflow!";
            std::terminate();
        }
        else if (previous_refcount < (INVALID_REF_COUNT_INTERVAL_START - 1U))
        {
            return AcquiredRefcountIndex{region_index,
                                         known_regions_versions_refcounts_.at(static_cast<size_t>(region_index))};
        }
        else
        {
            // no valid version acquired:
            // INVALID_REF_COUNT_INTERVAL_START <= previous_refcount < INVALID_REF_COUNT_INTERVAL_END
            continue;
        }
    }
    return std::nullopt;
}

template <template <class> class AtomicIndirectorType>
// coverity[autosar_cpp14_a15_5_3_violation] See rationale for std::array:at() autosar_cpp14_a15_5_3_violation above
std::optional<std::uint8_t> MemoryRegionMapImpl<AtomicIndirectorType>::AcquireRegionVersionForOverwrite() noexcept
{
    // Arbitrary retry value here. It is expected, that when checking all known regions versions, the writer
    // will find one being unused! Because readers are accessing only the latest for a very short time ...
    constexpr std::uint8_t max_retries = 10U;
    for (std::uint8_t retry_count = 0U; retry_count < max_retries; retry_count++)
    {
        // Iterate over version indices starting with the version directly after the current
        // latest_known_region_version_. That way we are checking the oldest version first, to have the lowest
        // probability of clashes with readers.
        for (std::uint8_t loop_idx = 1U; loop_idx < VERSION_COUNT; loop_idx++)
        {
            const auto version_idx = static_cast<std::uint8_t>(
                (loop_idx + latest_known_region_version_.load(std::memory_order_relaxed)) % VERSION_COUNT);
            RegionVersionRefCountType cur_ref_count =
                known_regions_versions_refcounts_.at(static_cast<size_t>(version_idx)).load();
            if (cur_ref_count == 0U)
            {
                if (AtomicIndirectorType<RegionVersionRefCountType>::compare_exchange_weak(
                        known_regions_versions_refcounts_.at(static_cast<size_t>(version_idx)),
                        cur_ref_count,
                        INVALID_REF_COUNT_INTERVAL_START,
                        std::memory_order_acq_rel))
                {
                    return version_idx;
                }
            }
            else if (cur_ref_count == INITIAL_REF_COUNT_VALUE)
            {
                return version_idx;
            }
            else
            {
                // no action needed - a "continue" would be optimized out and lead to missing code coverage
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return std::nullopt;
}

template class MemoryRegionMapImpl<memory::shared::AtomicIndirectorReal>;
template class MemoryRegionMapImpl<memory::shared::AtomicIndirectorMock>;

}  // namespace score::memory::shared::detail
