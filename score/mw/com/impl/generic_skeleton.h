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

#include <map>
#include <string_view>

namespace score::mw::com::impl
{

class SkeletonBinding;


class GenericSkeleton : public SkeletonBase
{
  public:
    /// @brief Creates a GenericSkeleton for a given instance specifier.
    /// @param specifier The instance specifier.
    /// @return A GenericSkeleton or an error.
    static Result<GenericSkeleton> Create(const InstanceSpecifier& specifier,
                                          MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent) noexcept;

    /// @brief Creates a GenericSkeleton for a given instance identifier.
    /// @param identifier The instance identifier.
    /// @return A GenericSkeleton or an error.
    static Result<GenericSkeleton> Create(const InstanceIdentifier& identifier,
                                          MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent) noexcept;

    /// @brief Adds a type-erased event to the skeleton.
    ///
    /// This must be called before OfferService().
    ///
    /// @param name The name of the event.
    /// @param size_info The size and alignment requirements for the event's sample data.
    /// @return A reference to the created event or an error.
    Result<GenericSkeletonEvent*> AddEvent(std::string_view name, const SizeInfo& size_info) noexcept;

    /// @brief Offers the service instance.
    /// @return A blank result, or an error if offering fails.
    Result<score::Blank> OfferService() noexcept;

    /// @brief Stops offering the service instance.
    void StopOfferService() noexcept;

  private:
    GenericSkeleton(const InstanceIdentifier& identifier,
                    std::unique_ptr<SkeletonBinding> binding,
                    MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent);

    std::map<std::string_view, std::unique_ptr<GenericSkeletonEvent>> owned_events_;
};

} // namespace score::mw::com::impl