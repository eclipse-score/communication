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
#ifndef SCORE_LIB_MEMORY_SHARED_MEMORYRESOURCEREGISTRY_H
#define SCORE_LIB_MEMORY_SHARED_MEMORYRESOURCEREGISTRY_H

#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/memory_region_bounds.h"
#include "score/memory/shared/memory_region_map.h"

#include "score/result/result.h"

#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

namespace score::memory::shared
{

namespace test
{
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// The forward declaration of MemoryResourceRegistryAttorney is necessary to avoid cyclic dependencies and to ensure
// that the ManagedMemoryResource class can be declared without requiring the full definition of
// MemoryResourceRegistryAttorney. This forward declaration does not violate the AUTOSAR C++14 M3-2-3 guideline as it is
// a common practice to handle dependencies in large codebases. The full definition of MemoryResourceRegistryAttorney
// will be provided elsewhere in the codebase.
// coverity[autosar_cpp14_m3_2_3_violation]
class MemoryResourceRegistryAttorney;
}  // namespace test

/**
 * @brief A singleton within a process to store all instances of an ManagedMemoryResource.
 *        It is a thread-safe (multiple read, single write) implementation.
 *
 *        It will be used by a MemoryResourceProxy to query its associated
 *        MemoryResource based on a common identifier.
 */
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// this is false positive. MemoryResourceRegistry is declared only once.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
class MemoryResourceRegistry final
{
  public:
    using MemoryResourceIdentifier = std::uint64_t;

    ~MemoryResourceRegistry() = default;
    MemoryResourceRegistry(const MemoryResourceRegistry& other) = delete;
    MemoryResourceRegistry(MemoryResourceRegistry&& other) noexcept = delete;
    MemoryResourceRegistry& operator=(const MemoryResourceRegistry& other) & = delete;
    MemoryResourceRegistry& operator=(MemoryResourceRegistry&& other) & noexcept = delete;

    static MemoryResourceRegistry& getInstance();
    ManagedMemoryResource* at(const MemoryResourceIdentifier) const noexcept;

    /// \brief insert managed memory resource under given key into registry.
    /// \return true, in case insert/registration was successful, false otherwise (existing element with same key)
    bool insert_resource(const std::pair<MemoryResourceIdentifier, ManagedMemoryResource*> input) noexcept;

    /// \brief remove_resource managed memory resource under given key (if it exists)
    void remove_resource(const MemoryResourceIdentifier identifier) noexcept;

    /// \brief removes ALL registered managed memory resources.
    /// \deprecated Currently only used as a test cleanup helper. There is no real "real world" use for now. We will
    ///             therefore remove_resource it soonish.
    void clear() noexcept;

    /// \brief Get the memory bounds of the memory resource in which the provided pointer resides, if there is one.
    ///        Internally converts the pointer to an integer and calls GetBoundsFromAddressAsInteger.
    /// \return Returns the memory bounds of a memory resource if the pointer resides within the bounds of a memory
    ///         resource that has been registered with the MemoryResourceRegistry. Otherwise, returns an empty optional.
    std::optional<MemoryRegionBounds> GetBoundsFromAddress(const void* const pointer) const noexcept;

    /// \brief Get the memory bounds of the memory resource in which the provided pointer resides, if there is one.
    /// \return Returns the memory bounds of a memory resource if the pointer resides within the bounds of a memory
    ///         resource that has been registered with the MemoryResourceRegistry. Otherwise, returns an empty optional.
    std::optional<MemoryRegionBounds> GetBoundsFromAddressAsInteger(
        const std::uintptr_t pointer_as_integer) const noexcept;

    /// \brief Get the memory bounds of the memory resource corresponding to the provided identifier.
    /// \return Returns the memory bounds of a memory resource if a resource corresponding to the provided identifier
    ///         has been registered with the MemoryResourceRegistry. Otherwise, returns an error.
    score::Result<MemoryRegionBounds> GetBoundsFromIdentifier(const MemoryResourceIdentifier identifier) const noexcept;

  private:
    static void CheckResourceInput(score::memory::shared::ManagedMemoryResource* const resource);

    // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used.".
    // The 'friend' class is employed to encapsulate non-public members.
    // This design choice protects end users from implementation details
    // and prevents incorrect usage. Friend classes provide controlled
    // access to private members, utilized internally, ensuring that
    // end users cannot access implementation specifics.
    // This is for testing only
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend test::MemoryResourceRegistryAttorney;

    /// \brief ctor is private because of singleton pattern.
    MemoryResourceRegistry() = default;

    /// \brief shared (reader/writer) mutex for synchronized access to registry_.
    mutable std::shared_timed_mutex mutex_{};
    std::unordered_map<MemoryResourceIdentifier, ManagedMemoryResource*> registry_{};

    MemoryRegionMap region_map_{};
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_MEMORYRESOURCEREGISTRY_H
