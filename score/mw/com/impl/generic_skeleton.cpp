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

#include <score/overload.hpp> 

#include <cassert>
#include <utility>

namespace score::mw::com::impl
{

namespace
{
// Helper to fetch the stable event name from the Configuration
std::string_view GetEventName(const InstanceIdentifier& identifier, std::string_view search_name)
{
    const auto& service_type_deployment = InstanceIdentifierView{identifier}.GetServiceTypeDeployment();

    auto visitor = score::cpp::overload(
        [&](const LolaServiceTypeDeployment& deployment) -> std::string_view {
            const auto it = deployment.events_.find(std::string{search_name});
            if (it != deployment.events_.end())
            {
                return it->first; // Return the stable address of the Key from the Config Map
            }
            return {};
        },
        [](const score::cpp::blank&) noexcept -> std::string_view {
            return {};
        }
    );

    return std::visit(visitor, service_type_deployment.binding_info_);
}
} // namespace

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

    // 2. Atomically create all events
    for (const auto& info : in.events) // Iterate directly over in.events
    {
        // Check for duplicates before creating the binding to prevent unnecessary work and potential memory issues.
        if (skeleton.events_.find(info.name) != skeleton.events_.cend())
        {
            score::mw::log::LogError("GenericSkeleton") << "Duplicate event name provided: " << info.name;
            return MakeUnexpected(ComErrc::kServiceElementAlreadyExists);
        }

        // 1. Fetch the STABLE Name from Configuration
        std::string_view stable_name = GetEventName(identifier, info.name);
        
        if (stable_name.empty())
        {
             score::mw::log::LogError("GenericSkeleton") << "Event name not found in configuration: " << info.name;
             return MakeUnexpected(ComErrc::kBindingFailure);
        }

        // Create the event binding
        auto event_binding_result =
            GenericSkeletonEventBindingFactory::Create(skeleton, info.name, info.data_type_meta_info);
        
        if (!event_binding_result.has_value())
        {
            // If any event fails to bind, the whole creation fails (Atomic). Error logging is typically handled within the factory.
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        // 2. Create object in Vector (Ownership & Stability)
        // Pass stable_name to constructor
        skeleton.owned_events_.push_back(std::make_unique<GenericSkeletonEvent>(
            skeleton, stable_name, std::move(event_binding_result).value()));

        // 3. Get Pointer to the stable object
        auto* event_ptr = skeleton.owned_events_.back().get();

        // 4. Store Pointer in Map using the stable_name retrieved from config
        skeleton.events_.emplace(stable_name, event_ptr);
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