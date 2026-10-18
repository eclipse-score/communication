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
#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory.h"
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
        [&](const SomeIpServiceTypeDeployment& deployment) -> std::string_view {
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

/// \brief Returns the field name from the configuration as a string_view, which is assured to stay valid for the
///        whole application lifecycle.
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
        [](const SomeIpServiceTypeDeployment&) noexcept -> std::string_view {
            return {};
        },
        [](const score::cpp::blank&) noexcept -> std::string_view {
            return {};
        });

    return std::visit(visitor, service_type_deployment.binding_info_);
}
}  // namespace

Result<GenericSkeleton> GenericSkeleton::Create(const InstanceSpecifier& specifier,
                                                const GenericSkeletonServiceElementInfo& in)
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
                                                const GenericSkeletonServiceElementInfo& in)
{
    auto binding = SkeletonBindingFactory::Create(identifier);
    if (!binding)
    {
        score::mw::log::LogError("GenericSkeleton") << "Failed to create SkeletonBinding for the given identifier.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    // 1. Create the Skeleton (Private Constructor)
    GenericSkeleton skeleton(identifier, std::move(binding));

    // 2. Create events
    const auto create_events_result = skeleton.CreateEvents(identifier, in.events);
    if (!create_events_result.has_value())
    {
        return MakeUnexpected<GenericSkeleton>(create_events_result.error());
    }

    // 3. Create fields
    const auto create_fields_result = skeleton.CreateFields(identifier, in.fields);
    if (!create_fields_result.has_value())
    {
        return MakeUnexpected<GenericSkeleton>(create_fields_result.error());
    }

    return skeleton;
}

Result<void> GenericSkeleton::CreateEvents(const InstanceIdentifier& identifier,
                                           score::cpp::span<const EventInfo> events)
{
    for (const auto& info : events)
    {
        if (events_->find(info.name) != events_->cend())
        {
            score::mw::log::LogError("GenericSkeleton") << "Duplicate event name provided: " << info.name;
            return MakeUnexpected(ComErrc::kServiceElementAlreadyExists);
        }

        std::string_view stable_name = GetEventName(identifier, info.name);

        if (stable_name.empty())
        {
            score::mw::log::LogError("GenericSkeleton") << "Event name not found in configuration: " << info.name;
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        auto data_type_size_info_result = MakeDataTypeSizeInfo(info.data_type_meta_info);
        if (!data_type_size_info_result.has_value())
        {
            score::mw::log::LogError("GenericSkeleton")
                << "Invalid data type meta-info provided for event: " << info.name;
            return MakeUnexpected(ComErrc::kInvalidConfiguration);
        }

        auto event_binding_ptr = SkeletonEventBindingFactory::Create(
            identifier, SkeletonBaseView{*this}.GetBinding(), info.name, data_type_size_info_result.value());

        if (!event_binding_ptr)
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        const auto emplace_result =
            events_->emplace(std::piecewise_construct,
                             std::forward_as_tuple(stable_name),
                             std::forward_as_tuple(*this, stable_name, std::move(event_binding_ptr)));

        if (!emplace_result.second)
        {
            score::mw::log::LogError("GenericSkeleton") << "Failed to emplace event in map: " << info.name;
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
    }
    return {};
}

Result<void> GenericSkeleton::CreateFields(const InstanceIdentifier& identifier,
                                           score::cpp::span<const FieldInfo> fields)
{
    for (const auto& info : fields)
    {
        if (fields_->find(info.name) != fields_->cend())
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

        auto data_type_size_info_result = MakeDataTypeSizeInfo(info.data_type_meta_info);
        if (!data_type_size_info_result.has_value())
        {
            score::mw::log::LogError("GenericSkeleton")
                << "Invalid data type meta-info provided for field: " << info.name;
            return MakeUnexpected(ComErrc::kInvalidConfiguration);
        }

        const auto emplace_result = fields_->emplace(
            std::piecewise_construct,
            std::forward_as_tuple(stable_name),
            std::forward_as_tuple(
                *this, stable_name, data_type_size_info_result.value(), info.has_getter, info.has_setter));

        if (!emplace_result.second)
        {
            score::mw::log::LogError("GenericSkeleton") << "Failed to emplace field in map: " << info.name;
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        if (!emplace_result.first->second.HasValidBinding())
        {
            score::mw::log::LogError("GenericSkeleton") << "Failed to create event binding for field: " << info.name;
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
    }
    return {};
}

ServiceElementMapView<GenericSkeletonEvent> GenericSkeleton::GetEvents() const noexcept
{
    return ServiceElementMapViewFactory<GenericSkeletonEvent>::Create(*events_);
}

GenericSkeleton::FieldMapView GenericSkeleton::GetFields() const noexcept
{
    return ServiceElementMapViewFactory<GenericSkeletonField>::Create(*fields_);
}

Result<void> GenericSkeleton::OfferService()
{
    return SkeletonBase::OfferService();
}

void GenericSkeleton::StopOfferService()
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
