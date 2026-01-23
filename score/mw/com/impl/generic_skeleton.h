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
#pragma once

#include "score/mw/com/impl/generic_skeleton_event.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/size_info.h"

#include "score/result/result.h"
#include <score/span.hpp> // Changed: Using your project's span

#include <vector>
#include <string_view>
#include <cstdint>

namespace score::mw::com::impl
{

class SkeletonBinding;

/// @brief Describes the configuration for a single event.
struct EventInfo
{
    std::string_view name;
    SizeInfo size_info;
};

/// @brief An opaque handle to an event within a GenericSkeleton.
struct EventHandle
{
    std::uint16_t index;
};

/// @brief A generic, type-erased skeleton implementation.
///
/// This class allows creating and offering service instances dynamically without
/// requiring compile-time generated code. It handles the lifecycle of events
/// and the service offering process.
class GenericSkeleton : public SkeletonBase
{
  public:
    /// @brief Creates a GenericSkeleton and all its events atomically.
    /// @param id The instance identifier.
    /// @param events A span of EventInfo structs describing all events to be created.
    /// @param out_handles A span that will be populated with handles to the created events.
    ///                    The caller must ensure its size is equal to events.size().
    /// @param mode The method call processing mode.
    /// @return A GenericSkeleton or an error if creation fails.
    static Result<GenericSkeleton> Create(const InstanceIdentifier& id,
                                          score::cpp::span<const EventInfo> events,
                                          score::cpp::span<EventHandle> out_handles,
                                          MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent) noexcept;

    /// @brief Creates a GenericSkeleton for a given instance specifier.
    /// Note: You likely need to update this overload to match the new atomic creation style 
    /// or remove it if it is no longer supported.
    static Result<GenericSkeleton> Create(const InstanceSpecifier& specifier,
                                          score::cpp::span<const EventInfo> events,
                                          score::cpp::span<EventHandle> out_handles,
                                          MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent) noexcept;

    /// @brief Retrieves an event using its handle.
    /// @param h The handle to the event.
    /// @return A reference to the GenericSkeletonEvent.
    GenericSkeletonEvent& GetEvent(EventHandle h) noexcept;

    /// @brief Offers the service instance.
    /// @return A blank result, or an error if offering fails.
    Result<score::Blank> OfferService() noexcept;

    /// @brief Stops offering the service instance.
    void StopOfferService() noexcept;

  private:
    GenericSkeleton(const InstanceIdentifier& identifier,
                    std::unique_ptr<SkeletonBinding> binding,
                    MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent);

    // Internal storage is now a vector for O(1) access via EventHandle.
    std::vector<std::unique_ptr<GenericSkeletonEvent>> owned_events_;
};

} // namespace score::mw::com::impl