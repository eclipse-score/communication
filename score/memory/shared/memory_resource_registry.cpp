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
#include "score/memory/shared/memory_resource_registry.h"

#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/memory/shared/shared_memory_error.h"

#include "score/mw/log/logging.h"

#include <score/utility.hpp>

#include <exception>
#include <optional>

auto score::memory::shared::MemoryResourceRegistry::getInstance() -> MemoryResourceRegistry&
{
    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized.".
    // Documentation and example for Rule A3-3-2 in
    // https://www.autosar.org/fileadmin/standards/R20-11/AP/AUTOSAR_RS_CPP14Guidelines.pdf show that
    // MemoryResourceRegistry would need a constexpr constructor to be compliant. This is not possible here
    // because MemoryResourceRegistry contains a std::shared_time_mutex and std::unordered_map which don't have
    // constexpr constructors.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static MemoryResourceRegistry instance{};
    return instance;
}

auto score::memory::shared::MemoryResourceRegistry::at(const MemoryResourceIdentifier identifier) const noexcept
    -> ManagedMemoryResource*
{
    std::shared_lock<std::shared_timed_mutex> lock{this->mutex_};
    const auto iterator = this->registry_.find(identifier);
    if (iterator == this->registry_.cend())
    {
        return nullptr;
    }
    return iterator->second;
}

auto score::memory::shared::MemoryResourceRegistry::insert_resource(
    const std::pair<MemoryResourceIdentifier, ManagedMemoryResource*> input) noexcept -> bool
{
    auto* const resource = input.second;
    if (resource == nullptr)
    {
        score::mw::log::LogFatal("shm")
            << "Could not insert resource into MemoryResourceRegistry: Input resource is a nullptr.";
        std::terminate();
    }

    std::lock_guard<std::shared_timed_mutex> lock{this->mutex_};

    auto result = this->registry_.insert(input);

    // If we successfully inserted the ID into the registry, add the memory_range to known_regions.
    if (!input.second->IsOffsetPtrBoundsCheckBypassingEnabled() && result.second)
    {
        void* const memory_range_start = resource->getBaseAddress();
        const void* const memory_range_end = resource->getEndAddress();

        if ((memory_range_start == nullptr) || (memory_range_end == nullptr))
        {
            score::mw::log::LogFatal("shm")
                << "Could not insert resource into MemoryResourceRegistry: The memory address range: ["
                << memory_range_start << ":" << memory_range_end << "cannot contain a nullptr.";
            std::terminate();
        }

        const auto memory_range_start_as_integer = CastPointerToInteger(memory_range_start);
        const auto memory_range_end_as_integer = CastPointerToInteger(memory_range_end);

        return region_map_.UpdateKnownRegion(memory_range_start_as_integer, memory_range_end_as_integer);
    }
    return result.second;
}

void score::memory::shared::MemoryResourceRegistry::remove_resource(const MemoryResourceIdentifier identifier) noexcept
{
    std::lock_guard<std::shared_timed_mutex> lock{this->mutex_};
    const auto resource_it = this->registry_.find(identifier);
    if (resource_it != this->registry_.cend())
    {
        if (!resource_it->second->IsOffsetPtrBoundsCheckBypassingEnabled())
        {
            const auto start_address = resource_it->second->getBaseAddress();
            const auto start_address_as_integer = CastPointerToInteger(start_address);
            region_map_.RemoveKnownRegion(start_address_as_integer);
        }
        score::cpp::ignore = this->registry_.erase(resource_it);
    }
}

auto score::memory::shared::MemoryResourceRegistry::clear() noexcept -> void
{
    std::lock_guard<std::shared_timed_mutex> lock{this->mutex_};
    this->registry_.clear();
    region_map_.ClearKnownRegions();
}

auto score::memory::shared::MemoryResourceRegistry::GetBoundsFromIdentifier(
    const MemoryResourceIdentifier identifier) const noexcept -> score::Result<MemoryRegionBounds>
{
    std::shared_lock<std::shared_timed_mutex> lock{this->mutex_};
    const auto resource_it = this->registry_.find(identifier);
    if (resource_it != this->registry_.cend())
    {
        const auto* const start_address = resource_it->second->getBaseAddress();
        const auto* const end_address = resource_it->second->getEndAddress();
        const auto start_address_as_integer = CastPointerToInteger(start_address);
        const auto end_address_as_integer = CastPointerToInteger(end_address);
        return {{start_address_as_integer, end_address_as_integer}};
    }
    return MakeUnexpected(SharedMemoryErrorCode::UnknownSharedMemoryIdentifier);
}

auto score::memory::shared::MemoryResourceRegistry::GetBoundsFromAddress(const void* const pointer) const noexcept
    -> std::optional<MemoryRegionBounds>
{
    const auto pointer_as_integer = CastPointerToInteger(pointer);
    return region_map_.GetBoundsFromAddress(pointer_as_integer);
}

auto score::memory::shared::MemoryResourceRegistry::GetBoundsFromAddressAsInteger(
    const std::uintptr_t pointer_as_integer) const noexcept -> std::optional<MemoryRegionBounds>
{
    return region_map_.GetBoundsFromAddress(pointer_as_integer);
}
