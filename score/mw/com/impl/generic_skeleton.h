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
// #include "score/mw/com/impl/generic_skeleton_field_binding.h" // New include - commented out as field not implemented
// #include "score/mw/com/impl/generic_skeleton_field.h" // commented out as field not implemented
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/service_element_map.h"
#include "score/result/result.h"
#include <score/span.hpp> 

#include <vector>
#include <string_view>
#include <cstdint>
#include <memory>

namespace score::mw::com::impl
{

struct EventInfo
{
    std::string_view name;
    DataTypeMetaInfo data_type_meta_info;
};
// struct FieldInfo // commented out as field not implemented
// {
//     std::string_view name;
//      DataTypeMetaInfo data_type_meta_info;
//     /// @brief The initial value for the field.
//     /// @note Must be non-empty.
//     /// @note The data must remain valid only for the duration of the Create() call.
//     /// `initial_value_bytes.size()` must be less than or equal to `size_info.size`.
//     /// The bytes are in the middleware’s “generic” field representation.
//     score::cpp::span<const std::uint8_t> initial_value_bytes;
//
// };
struct GenericSkeletonCreateParams
{
    score::cpp::span<const EventInfo> events{};
    //score::cpp::span<const FieldInfo> fields{};
};

/// @brief Represents a type-erased, runtime-configurable skeleton for a service instance.
///
/// A `GenericSkeleton` is created at runtime based on configuration data. It manages
/// a collection of `GenericSkeletonEvent` and `GenericSkeletonField` instances.

class GenericSkeleton : public SkeletonBase
{
  public:
    using EventMap = ServiceElementMap<GenericSkeletonEvent*>;
//      using FieldMap = ServiceElementMap<GenericSkeletonField>; // commented out as field not implemented
/// @brief Creates a GenericSkeleton and all its service elements (events + fields) atomically.
///
/// @contract
/// - Empty spans are allowed for `in.events` and/or `in.fields`
/// - Each provided name must exist in the binding deployment for this instance (events/fields respectively).
/// - All element names must be unique across all element kinds within this skeleton.
/// - For each field, `initial_value_bytes` must be non-empty and
///   `initial_value_bytes.size()` must be <= `size_info.size`.
/// - On error, no partially-created elements are left behind.
[[nodiscard]] static Result<GenericSkeleton> Create(
    const InstanceIdentifier& identifier,
    const GenericSkeletonCreateParams& in,
    MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent) noexcept;

/// @brief Same as Create(InstanceIdentifier, ...) but resolves the specifier first. 
/// @param specifier The instance specifier.
/// @param in Input parameters for creation.
/// @param mode The method call processing mode.
/// @return A GenericSkeleton or an error.
 [[nodiscard]] static Result<GenericSkeleton> Create(
    const InstanceSpecifier& specifier,
    const GenericSkeletonCreateParams& in,
    MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent) noexcept;

/// @brief Returns a const reference to the name-keyed map of events.
/// @note The returned reference is valid as long as the GenericSkeleton lives. 
[[nodiscard]] const EventMap& GetEvents() const noexcept;


/// @brief Returns a const reference to the name-keyed map of fields.
/// @note The returned reference is valid as long as the GenericSkeleton lives. 
//[[nodiscard]] const FieldMap& GetFields() const noexcept;

/// @brief Offers the service instance.
/// @return A blank result, or an error if offering fails.
[[nodiscard]] Result<score::Blank> OfferService() noexcept;

/// @brief Stops offering the service instance.
void StopOfferService() noexcept;
  private:
    // Private constructor, only callable by static Create methods.
    GenericSkeleton(const InstanceIdentifier& identifier, std::unique_ptr<SkeletonBinding> binding, MethodCallProcessingMode mode);

    std::vector<std::unique_ptr<GenericSkeletonEvent>> owned_events_;
    /// @brief This map owns all GenericSkeletonEvent instances.
    EventMap events_;

    /// @brief This map owns all GenericSkeletonField instances.
//    FieldMap fields_; // commented out as field not implemented
};
} // namespace score::mw::com::impl