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
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/service_element_map_view_factory.h"
#include "score/mw/com/impl/skeleton_binding.h"

#include <score/overload.hpp>

#include <cassert>
#include <tuple>
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
                return it->first;  // Return the stable address of the Key from the Config Map
            }
            return {};
        },
        [](const score::cpp::blank&) noexcept -> std::string_view {
            return {};
        });

    return std::visit(visitor, service_type_deployment.binding_info_);
}

// Helper to fetch the stable field name from the Configuration
std::string_view GetFieldName(const InstanceIdentifier& identifier, std::string_view search_name)
{
    const auto& service_type_deployment = InstanceIdentifierView{identifier}.GetServiceTypeDeployment();

    auto visitor = score::cpp::overload(
        [&](const LolaServiceTypeDeployment& deployment) -> std::string_view {
            const auto it = deployment.fields_.find(std::string{search_name});
            if (it != deployment.fields_.end())
            {
                return it->first;  // Return the stable address of the Key from the Config Map
            }
            return {};
        },
        [](const score::cpp::blank&) noexcept -> std::string_view {
            return {};
        });

    return std::visit(visitor, service_type_deployment.binding_info_);
}
}  // namespace

Result<GenericSkeleton> GenericSkeleton::Create(const InstanceSpecifier& specifier,
                                                const GenericSkeletonServiceElementInfo& in) noexcept
{
    const auto instance_identifier_result = GetInstanceIdentifier(specifier);

    if (!instance_identifier_result.has_value())
    {
        score::mw::log::LogError("GenericSkeleton") << "Failed to resolve instance identifier from instance specifier";
        return MakeUnexpected(ComErrc::kInstanceIDCouldNotBeResolved);
    }

    return Create(instance_identifier_result.value(), in);
}

Result<GenericSkeleton> GenericSkeleton::Create(const InstanceIdentifier& identifier,
                                                const GenericSkeletonServiceElementInfo& in) noexcept
{
    auto binding = SkeletonBindingFactory::Create(identifier);
    if (!binding)
    {

        score::mw::log::LogError("GenericSkeleton") << "Failed to create SkeletonBinding for the given identifier.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    // 1. Create the Skeleton (Private Constructor)
    GenericSkeleton skeleton(identifier, std::move(binding));

    // 2. Create events directly in the map
    for (const auto& info : in.events)
    {
        // Check for duplicates
        if (skeleton.events_->find(info.name) != skeleton.events_->cend())
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

        auto event_binding_result =
            GenericSkeletonEventBindingFactory::Create(skeleton, info.name, info.data_type_meta_info);

        if (!event_binding_result.has_value())
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        const auto emplace_result = skeleton.events_->emplace(
            std::piecewise_construct,
            std::forward_as_tuple(stable_name),
            std::forward_as_tuple(skeleton, stable_name, std::move(event_binding_result).value()));

        if (!emplace_result.second)
        {
            score::mw::log::LogError("GenericSkeleton") << "Failed to emplace event in map: " << info.name;
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
    }

    // 3. Create fields directly in the map
    for (const auto& info : in.fields)
    {
        // Check for duplicates
        if (skeleton.fields_->find(info.name) != skeleton.fields_->cend())
        {
            score::mw::log::LogError("GenericSkeleton") << "Duplicate field name provided: " << info.name;
            return MakeUnexpected(ComErrc::kServiceElementAlreadyExists);
        }

        std::string_view stable_name = GetFieldName(identifier, info.name);

        if (stable_name.empty())
        {
            score::mw::log::LogError("GenericSkeleton") << "Field name not found in configuration: " << info.name;
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        auto field_binding_result =
            GenericSkeletonEventBindingFactory::Create(skeleton, info.name, info.data_type_meta_info);

        if (!field_binding_result.has_value())
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        // Use the hidden constructor tag so the event doesn't register itself in the events_ map
        auto generic_event =
            std::make_unique<GenericSkeletonEvent>(skeleton,
                                                   stable_name,
                                                   std::move(field_binding_result).value(),
                                                   GenericSkeletonEvent::FieldOnlyConstructorEnabler{});

        const auto emplace_result = skeleton.fields_->emplace(
            std::piecewise_construct,
            std::forward_as_tuple(stable_name),
            std::forward_as_tuple(
                skeleton, stable_name, std::move(generic_event), info.has_getter, info.has_setter, info.has_notifier));

        if (!emplace_result.second)
        {
            score::mw::log::LogError("GenericSkeleton") << "Failed to emplace field in map: " << info.name;
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        auto update_result = emplace_result.first->second.Update(info.initial_value);
        if (!update_result.has_value())
        {
            score::mw::log::LogError("GenericSkeleton") << "Failed to set initial value for field: " << info.name;
            return score::Unexpected(update_result.error());
        }
    }

    return skeleton;
}

ServiceElementMapView<GenericSkeletonEvent> GenericSkeleton::GetEvents() const noexcept
{
    return ServiceElementMapViewFactory<GenericSkeletonEvent>::Create(*events_);
}

GenericSkeleton::FieldMapView GenericSkeleton::GetFields() const noexcept
{
    return ServiceElementMapViewFactory<GenericSkeletonField>::Create(*fields_);
}

Result<void> GenericSkeleton::OfferService() noexcept
{
    return SkeletonBase::OfferService();
}

void GenericSkeleton::StopOfferService() noexcept
{
    SkeletonBase::StopOfferService();
}

GenericSkeleton::GenericSkeleton(const InstanceIdentifier& identifier, std::unique_ptr<SkeletonBinding> binding)
    : SkeletonBase(std::move(binding), identifier),
      events_(std::make_unique<std::map<std::string_view, GenericSkeletonEvent>>()),
      fields_(std::make_unique<std::map<std::string_view, GenericSkeletonField>>())
{
}

}  // namespace score::mw::com::impl
