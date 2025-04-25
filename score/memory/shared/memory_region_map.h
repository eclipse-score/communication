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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_MEMORYREGIONMAP_H
#define BASELIBS_SCORE_MEMORY_SHARED_MEMORYREGIONMAP_H

#include "score/memory/shared/atomic_indirector.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <utility>

namespace score::memory::shared
{

namespace test
{
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// The forward declaration of MemoryRegionMapAttorney is necessary to avoid cyclic dependencies and to ensure that the
// MemoryRegionMapAttorney class can be declared without requiring the full definition of MemoryRegionMapAttorney. This
// forward declaration does not violate the AUTOSAR C++14 M3-2-3 guideline as it is a common practice to handle
// dependencies in large codebases. The full definition of MemoryRegionMapAttorney will be provided elsewhere in the
// codebase.
// coverity[autosar_cpp14_m3_2_3_violation]
class MemoryRegionMapAttorney;
}  // namespace test

namespace detail
{

/// \brief A map for memory regions, which provides lock-free access to one writer and multiple concurrent readers
/// \details The non-const public APIs for writer access (insert new region, removal of a region, clearing regions)
///          are not thread-safe/re-entrant. I.e. access from multiple writers needs to be serialized. Opposed to that,
///          multiple readers can concurrently access the map also parallel to a writer.
///          Lock-Free algo is based on multi-versioning/swap mechanism in conjunction with atomics.
///          Detailed description can be found here:
///          platform/aas/lib/memory/design/shared_memory/Readme.md - chapter Bounds_Checking_Performance

template <template <class> class AtomicIndirectorType = memory::shared::AtomicIndirectorReal>
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// this is false positive. MemoryRegionMapImpl is declared only once.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
class MemoryRegionMapImpl final
{
    // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used.".
    // The 'friend' class is employed to encapsulate non-public members.
    // This design choice protects end users from implementation details
    // and prevents incorrect usage. Friend classes provide controlled
    // access to private members, utilized internally, ensuring that
    // end users cannot access implementation specifics.
    // This is for testing only
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend test::MemoryRegionMapAttorney;

  public:
    using MemoryResourceIdentifier = std::uint64_t;
    using MemoryBounds = std::pair<std::uintptr_t, std::uintptr_t>;

    MemoryRegionMapImpl() noexcept;

    /// \brief Searches within the existing/known memory regions for a region, which contains the given pointer/address.
    /// \param pointer ptr/address to be checked
    /// \return either a pair of start-address/end-address of the region which contains the pointer or an empty value
    ///         if there is no known region containing this pointer.
    std::optional<std::pair<MemoryBounds, MemoryResourceIdentifier>> GetBoundsFromAddress(
        const std::uintptr_t pointer) const noexcept;

    /// \brief Creates a new regions version based on the latest version and adds the given region to it and then marks
    ///        it as the latest regions version. Only adds the provided range if it doesn't overlap with another range
    ///        that was already added via UpdateKnownRegions
    /// \param memory_range_start
    /// \param memory_range_end
    /// \param memory_resource_identifier identifier for identifying the memory resource in the MemoryResourceRegistry
    /// \return true, if the range was inserted. Otherwise, false.
    bool UpdateKnownRegion(const std::uintptr_t memory_range_start,
                           const std::uintptr_t memory_range_end,
                           const MemoryResourceIdentifier memory_resource_identifier) noexcept;

    /// \brief Removes the region with the given start address from the current/latest regions version and creates a new
    ///        regions version from it and marks it as the latest one.
    /// \param memory_range_start
    void RemoveKnownRegion(const std::uintptr_t memory_range_start) noexcept;

    /// \brief Creates a new regions version, which is empty (contains no regions) and marks it as the latest regions
    /// version.
    /// \deprecated This func is only called/used by MemoryResourceRegistry::clear(), which is marked as deprecated.
    void ClearKnownRegions() noexcept;

    /// \brief returns the size (number of regions) of the latest/newest known region version
    /// \return size/number of known regions.
    std::size_t GetSize() const noexcept;

  private:
    using RegionVersionRefCountType = std::uint32_t;

    /// \brief RAII style wrapper around an acquired atomic ref-counter, which assures refcount decrement on
    /// destruction.
    class AcquiredRefcountIndex final
    {
      public:
        AcquiredRefcountIndex(const std::uint8_t refcount_index,
                              std::atomic<RegionVersionRefCountType>& ref_count) noexcept;
        ~AcquiredRefcountIndex() noexcept;
        AcquiredRefcountIndex(const AcquiredRefcountIndex& other) noexcept = delete;
        /// \brief although the contained atomic isn't movable, our wrapper needs to be movable as we intend to use it
        ///        with e.g. std::optional. So we implement specific move logic here!
        AcquiredRefcountIndex(AcquiredRefcountIndex&& other) noexcept;
        AcquiredRefcountIndex& operator=(const AcquiredRefcountIndex& other) & = delete;
        AcquiredRefcountIndex& operator=(AcquiredRefcountIndex&& other) & = delete;

        std::uint8_t GetIndex() const noexcept
        {
            return index_;
        }

      private:
        std::uint8_t index_;
        std::reference_wrapper<std::atomic<RegionVersionRefCountType>> ref_count_;
        bool owns_resource_;
    };
    static constexpr const RegionVersionRefCountType INVALID_REF_COUNT_INTERVAL_END{
        std::numeric_limits<RegionVersionRefCountType>::max()};
    static constexpr const RegionVersionRefCountType INVALID_REF_COUNT_INTERVAL_START{INVALID_REF_COUNT_INTERVAL_END /
                                                                                      2U};
    static constexpr const RegionVersionRefCountType INITIAL_REF_COUNT_VALUE{INVALID_REF_COUNT_INTERVAL_START};
    // Suppress "AUTOSAR C++14 A0-1-1" rule finding: "A project shall not contain instances of non-volatile variables
    // being given values that are not subsequently used.".
    // Rationale: False positive - variable is used below.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    static constexpr const std::uint8_t VERSION_COUNT{10U};
    static_assert(
        VERSION_COUNT <= 255U,
        "VERSION_COUNT needs to be smaller than 255 as our latest_known_region_version_ tracker is an uint8 ");

    /// \brief Increases the usage refcount of the latest regions version.
    /// \return returns the index (version) of the regions version, were it successfully incremented the refcount or an
    /// std::nullopt in case the refcount increment failed (which is a clear error/problem)
    auto AcquireLatestRegionVersionForRead() const noexcept -> std::optional<AcquiredRefcountIndex>;

    /// \brief Searches for an unused regions version and acquires/blocks it for overwriting with a newer version.
    /// \return returns the index (version) of the regions version, which got locked or an std::nullopt in case the
    /// refcount increment failed (which is a clear error/problem)
    std::optional<std::uint8_t> AcquireRegionVersionForOverwrite() noexcept;

    /// \brief We have an array of different versions of known regions to support our lock-free CAS based read access
    std::array<std::map<std::uintptr_t, std::pair<std::uintptr_t, MemoryResourceIdentifier>>, VERSION_COUNT>
        known_regions_versions_{};

    /// \brief refcounters for the different versions of known regions in known_regions_versions_
    mutable std::array<std::atomic<RegionVersionRefCountType>, VERSION_COUNT> known_regions_versions_refcounts_{};
    static_assert(VERSION_COUNT > 0U, "known_regions_versions_refcounts_ must store at least 1 version!");

    /// \brief index into known_regions_versions_, which represents the latest/newest version of known regions
    std::atomic_uint8_t latest_known_region_version_{0U};
};

}  // namespace detail

using MemoryRegionMap = detail::MemoryRegionMapImpl<>;

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_MEMORYREGIONMAP_H
