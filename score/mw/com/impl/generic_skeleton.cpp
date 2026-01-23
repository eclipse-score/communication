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
#include "score/mw/com/impl/generic_skeleton.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/size_info.h"
#include "score/mw/com/impl/skeleton_binding.h"

#include <cassert>
#include <utility>

namespace score::mw::com::impl
{

Result<GenericSkeleton> GenericSkeleton::Create(
    const InstanceSpecifier& specifier,
    score::cpp::span<const EventInfo> events,
    score::cpp::span<EventHandle> out_handles,
    MethodCallProcessingMode mode) noexcept
{
    const auto instance_identifier_result = GetInstanceIdentifier(specifier);

    if (!instance_identifier_result.has_value())
    {
        score::mw::log::LogError("GenericSkeleton") << "Failed to resolve instance identifier from instance specifier";
        return MakeUnexpected(ComErrc::kInstanceIDCouldNotBeResolved);
    }

    return Create(instance_identifier_result.value(), events, out_handles, mode);
}

Result<GenericSkeleton> GenericSkeleton::Create(
    const InstanceIdentifier& identifier,
    score::cpp::span<const EventInfo> events,
    score::cpp::span<EventHandle> out_handles,
    MethodCallProcessingMode mode) noexcept
{
    // Ensure the output span is large enough to hold handles for all events
    assert(events.size() == out_handles.size());

    auto binding = SkeletonBindingFactory::Create(identifier);
    if (!binding)
    {
       
        score::mw::log::LogError("GenericSkeleton") << "Failed to create SkeletonBinding for the given identifier.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    // 1. Create the Skeleton (Private Constructor)
    GenericSkeleton skeleton(identifier, std::move(binding), mode);

    // 2. Reserve space for events to avoid reallocation
    skeleton.owned_events_.reserve(events.size());

    // 3. Atomically create all events
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        const auto& info = events[i];

        // Create the event binding
        auto event_binding_result = GenericSkeletonEventBindingFactory::Create(skeleton, info.name, info.size_info);
        
        if (!event_binding_result.has_value())
        {
            // If any event fails to bind, the whole creation fails (Atomic)
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        // Store the event in the vector
        skeleton.owned_events_.push_back(std::make_unique<GenericSkeletonEvent>(
            skeleton, info.name, std::move(event_binding_result).value()));

        // Assign the handle (index in the vector)
        out_handles[i] = EventHandle{static_cast<std::uint16_t>(i)};
    }

    return skeleton;
}

GenericSkeletonEvent& GenericSkeleton::GetEvent(EventHandle h) noexcept
{
    // Fast O(1) lookup
    assert(h.index < owned_events_.size());
    return *owned_events_[h.index];
}

Result<score::Blank> GenericSkeleton::OfferService() noexcept
{
    return SkeletonBase::OfferService();
}

void GenericSkeleton::StopOfferService() noexcept
{
    SkeletonBase::StopOfferService();
}

GenericSkeleton::GenericSkeleton(const InstanceIdentifier& identifier,
                                 std::unique_ptr<SkeletonBinding> binding,
                                 MethodCallProcessingMode mode)
    : SkeletonBase(std::move(binding), identifier, mode)
{
}

} // namespace score::mw::com::impl