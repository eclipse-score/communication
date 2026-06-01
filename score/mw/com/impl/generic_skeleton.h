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
#ifndef SCORE_MW_COM_IMPL_GENERIC_SKELETON_H_
#define SCORE_MW_COM_IMPL_GENERIC_SKELETON_H_

#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/generic_skeleton_event.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_map_view.h"
#include "score/mw/com/impl/service_element_map_view_factory.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/result/result.h"
#include <score/span.hpp>

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

struct EventInfo
{
    std::string_view name;
    DataTypeMetaInfo data_type_meta_info;
};

struct GenericSkeletonServiceElementInfo
{
    score::cpp::span<const EventInfo> events{};
};

/// @brief Represents a type-erased, runtime-configurable skeleton for a service instance.
///
/// A `GenericSkeleton` is created at runtime based on configuration data. It manages
/// a collection of `GenericSkeletonEvent` and `GenericSkeletonField` instances.

class GenericSkeleton : public SkeletonBase
{
  public:
    using EventMapView = ServiceElementMapView<GenericSkeletonEvent>;
    /// \brief Creates a GenericSkeleton and all its service elements (events + fields) atomically.
    ///
    /// \contract
    /// - Empty spans are allowed for `in.events` and/or `in.fields`
    /// - Each provided name must exist in the binding deployment for this instance (events/fields respectively).
    /// - All element names must be unique across all element kinds within this skeleton.
    /// - For each field, `initial_value_bytes` must be non-empty and
    ///   `initial_value_bytes.size()` must be <= `size_info.size`.
    /// - On error, no partially-created elements are left behind.
    [[nodiscard]] static Result<GenericSkeleton> Create(const InstanceIdentifier& identifier,
                                                        const GenericSkeletonServiceElementInfo& in) noexcept;

    /// \brief Same as Create(InstanceIdentifier, ...) but resolves the specifier first.
    /// \param specifier The instance specifier.
    /// \param in Input parameters for creation.
    /// \return A GenericSkeleton or an error.
    [[nodiscard]] static Result<GenericSkeleton> Create(const InstanceSpecifier& specifier,
                                                        const GenericSkeletonServiceElementInfo& in) noexcept;

    /// \brief Returns a read-only view to the name-keyed map of events.
    /// \note The returned view is valid as long as the GenericSkeleton lives.
    [[nodiscard]] EventMapView GetEvents() const noexcept;

    /// \brief Offers the service instance.
    /// \return A blank result, or an error if offering fails.
    [[nodiscard]] Result<void> OfferService() noexcept;

    /// \brief Stops offering the service instance.
    void StopOfferService() noexcept;

  private:
    // Private constructor, only callable by static Create methods.
    GenericSkeleton(const InstanceIdentifier& identifier, std::unique_ptr<SkeletonBinding> binding);

    /// \brief This map owns all GenericSkeletonEvent instances.
    /// \details This map needs to be covered in a unique_ptr as it shall not be relocated by a move of the
    /// GenericSkeleton. This is required as we hand out views to this map (see GetEvents()), which need to be valid
    /// even after the GenericSkeleton instance has been moved.
    std::unique_ptr<ServiceElementMapViewFactory<GenericSkeletonEvent>::map_type> events_;
};
}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_SKELETON_H_
