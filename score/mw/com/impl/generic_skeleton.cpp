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
//#include "score/mw/com/impl/plumbing/generic_skeleton_field_binding_factory.h"  //not implemented yet
#include "score/mw/com/impl/runtime.h" 
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h" 
#include "score/mw/com/impl/skeleton_binding.h"

#include <cassert>
#include <utility>

namespace score::mw::com::impl
{

Result<GenericSkeleton> GenericSkeleton::Create(
    const InstanceSpecifier& specifier,
    const GenericSkeletonCreateParams& in,
    MethodCallProcessingMode mode) noexcept
{
    const auto instance_identifier_result = GetInstanceIdentifier(specifier);

    if (!instance_identifier_result.has_value())
    {
        score::mw::log::LogError("GenericSkeleton") << "Failed to resolve instance identifier from instance specifier";
        return MakeUnexpected(ComErrc::kInstanceIDCouldNotBeResolved);
    }

    return Create(instance_identifier_result.value(), in, mode); 
}

Result<GenericSkeleton> GenericSkeleton::Create(
    const InstanceIdentifier& identifier,
    const GenericSkeletonCreateParams& in,
    MethodCallProcessingMode mode) noexcept
{
    auto binding = SkeletonBindingFactory::Create(identifier);
    if (!binding)
    {
       
        score::mw::log::LogError("GenericSkeleton") << "Failed to create SkeletonBinding for the given identifier.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    // 1. Create the Skeleton (Private Constructor)
    GenericSkeleton skeleton(identifier, std::move(binding), mode);

    // 3. Atomically create all events
    for (const auto& info : in.events) // Iterate directly over in.events
    {
        // Create the event binding
        auto event_binding_result =
            GenericSkeletonEventBindingFactory::Create(skeleton, info.name, info.data_type_meta_info);
        
        if (!event_binding_result.has_value())
        {
            // If any event fails to bind, the whole creation fails (Atomic). Error logging is typically handled within the factory.
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        // Store the event in the map
        const auto emplace_result = skeleton.events_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(info.name),
            std::forward_as_tuple(skeleton, info.name, std::move(event_binding_result).value()));

        if (!emplace_result.second) {
            score::mw::log::LogError("GenericSkeleton") << "Failed to emplace GenericSkeletonEvent for name: " << info.name;
            return MakeUnexpected(ComErrc::kServiceElementAlreadyExists);
        }
    }

    // // 4. Atomically create all fields
    // for (const auto& info : in.fields) // Iterate directly over in.fields
    // {
    //     // Create the field binding
    //     auto field_binding_result =
    //         GenericSkeletonFieldBindingFactory::Create(skeleton, info.name, info.data_type_meta_info);

    //     if (!field_binding_result.has_value())
    //     {
    //         // If any field fails to bind, the whole creation fails (Atomic). Error logging is typically handled within the factory.
    //         return MakeUnexpected(ComErrc::kBindingFailure);
    //     }

    //     // Store the field in the map
    //     const auto emplace_result = skeleton.fields_.emplace(
    //         std::piecewise_construct,
    //         std::forward_as_tuple(info.name),
    //         std::forward_as_tuple(skeleton, info.name, std::move(field_binding_result).value(), info.initial_value_bytes));

    //     if (!emplace_result.second) {
    //         score::mw::log::LogError("GenericSkeleton") << "Failed to emplace GenericSkeletonField for name: " << info.name;
    //         return MakeUnexpected(ComErrc::kServiceElementAlreadyExists);
    //     }
    // }

    return skeleton;
}

const GenericSkeleton::EventMap& GenericSkeleton::GetEvents() const noexcept
{
    return events_;
}

GenericSkeleton::EventMap& GenericSkeleton::GetEvents() noexcept
{
    return events_;
}

// const GenericSkeleton::FieldMap& GenericSkeleton::GetFields() const noexcept
// {
//     return fields_;
// }

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