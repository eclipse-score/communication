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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SKELETON_SERVICE_ELEMENT_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_SKELETON_SERVICE_ELEMENT_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_common.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/configuration/binding_service_type_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/field_tags_store.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/overload.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

namespace score::mw::com::impl
{

namespace detail
{

/// \brief Creates a SkeletonEventProperties object from the configuration and field tags (for a field)
///
/// For an event, the number of slots and max subscribers comes purely from the configuration. For a field, we also take
/// into account the field tags when calculating the number of slots that are needed.
inline lola::SkeletonEventProperties CreateSkeletonEventProperties(
    const LolaEventInstanceDeployment& lola_event_instance_deployment,
    const std::optional<FieldTagsStore> field_tags_store)
{
    std::size_t max_subscribers{0U};

    if (!lola_event_instance_deployment.GetNumberOfSampleSlotsExcludingTracingSlot().has_value())
    {
        score::mw::log::LogFatal("lola") << "Could not create SkeletonEventProperties from "
                                            "ServiceElementInstanceDeployment. Number of sample slots "
                                            "was not specified in the configuration. Terminating.";
        std::terminate();
    }
    const auto number_of_slots = lola_event_instance_deployment.GetNumberOfSampleSlotsExcludingTracingSlot().value();

    // If we are creating SkeletonEventProperties for an event of for a field with a notifier, then the user must
    // provide the number of slots and number of subscribers in the configuration.
    const bool is_event = !field_tags_store.has_value();
    const bool is_field_with_subscription_semantics = !is_event && field_tags_store->HasNotifier();
    if (is_field_with_subscription_semantics || is_event)
    {
        if (!lola_event_instance_deployment.max_subscribers_.has_value())
        {
            score::mw::log::LogFatal("lola") << "Could not create SkeletonEventProperties from "
                                                "ServiceElementInstanceDeployment. Max subscribers was "
                                                "not specified in the configuration. Terminating.";
            std::terminate();
        }

        max_subscribers = lola_event_instance_deployment.max_subscribers_.value();
    }
    else
    {
        if (lola_event_instance_deployment.max_subscribers_.has_value())
        {
            score::mw::log::LogWarn("lola") << "Field has WithNotifier disabled; configured maxSubscribers is ignored.";
        }
    }

    const bool is_getter_enabled = !is_event && field_tags_store->HasGetter();
    const auto number_of_field_getter_slots = is_getter_enabled ? lola::kMaxConcurrentFieldGetterSamplePtrs : 0U;

    const bool is_setter_enabled = !is_event && field_tags_store->HasSetter();

    return lola::SkeletonEventProperties{number_of_slots,
                                         lola_event_instance_deployment.GetNumberOfTracingSlots(),
                                         number_of_field_getter_slots,
                                         is_setter_enabled,
                                         max_subscribers,
                                         lola_event_instance_deployment.enforce_max_samples_};
}

inline lola::SkeletonEventProperties CreateSkeletonEventProperties(
    const LolaFieldInstanceDeployment& lola_field_instance_deployment,
    const std::optional<FieldTagsStore> field_tags_store)
{
    return CreateSkeletonEventProperties(lola_field_instance_deployment.lola_event_instance_deployment_,
                                         field_tags_store);
}

}  // namespace detail

template <typename SkeletonServiceElementBinding, typename SkeletonServiceElement, ServiceElementType element_type>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto CreateSkeletonEventOrField(const InstanceIdentifier& identifier,
                                SkeletonBinding& parent_binding,
                                const std::string_view service_element_name,
                                std::optional<FieldTagsStore> field_tags_store) noexcept
    -> std::unique_ptr<SkeletonServiceElementBinding>
{
    static_assert((element_type == ServiceElementType::EVENT) || (element_type == ServiceElementType::FIELD));
    if constexpr (element_type == ServiceElementType::FIELD)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION(field_tags_store.has_value());
    }
    else if constexpr (element_type == ServiceElementType::EVENT)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION(!field_tags_store.has_value());
    }

    if (field_tags_store.has_value())
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION(field_tags_store->HasGetter() || field_tags_store->HasNotifier());
    }

    const InstanceIdentifierView identifier_view{identifier};

    using ReturnType = std::unique_ptr<SkeletonServiceElementBinding>;
    auto visitor = score::cpp::overload(
        [identifier_view, &parent_binding, &service_element_name, field_tags_store](
            const LolaServiceTypeDeployment& lola_service_type_deployment) -> ReturnType {
            auto* const lola_parent = dynamic_cast<lola::Skeleton*>(&parent_binding);
            if (lola_parent == nullptr)
            {
                score::mw::log::LogFatal("lola") << "Skeleton service element could not be created because parent "
                                                    "skeleton binding is a nullptr.";
                return nullptr;
            }

            const auto& service_instance_deployment = identifier_view.GetServiceInstanceDeployment();
            const auto& lola_service_instance_deployment =
                GetServiceInstanceDeploymentBinding<LolaServiceInstanceDeployment>(service_instance_deployment);

            const std::string service_element_name_str{service_element_name};
            const auto& lola_service_element_instance_deployment = GetServiceElementInstanceDeployment<element_type>(
                lola_service_instance_deployment, service_element_name_str);

            const lola::SkeletonEventProperties skeleton_event_properties =
                detail::CreateSkeletonEventProperties(lola_service_element_instance_deployment, field_tags_store);

            const auto lola_service_element_id =
                GetServiceElementId<element_type>(lola_service_type_deployment, service_element_name_str);
            const lola::ElementFqId element_fq_id{lola_service_type_deployment.service_id_,
                                                  lola_service_element_id,
                                                  lola_service_instance_deployment.instance_id_.value().GetId(),
                                                  element_type};

            return std::make_unique<SkeletonServiceElement>(*lola_parent,
                                                            element_fq_id,
                                                            service_element_name,
                                                            skeleton_event_properties,
                                                            impl::tracing::SkeletonEventTracingData{});
        },
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return nullptr;
        });

    return std::visit(visitor, identifier_view.GetServiceTypeDeployment().binding_info_);
}

/// @brief Overload for typed skeletons (which do not have a DataTypeMetaInfo).
template <typename SkeletonServiceElementBinding, typename SkeletonServiceElement, ServiceElementType element_type>
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto CreateGenericSkeletonEventOrField(const InstanceIdentifier& identifier,
                                       SkeletonBase& parent,
                                       const std::string_view service_element_name,
                                       const DataTypeMetaInfo& meta_info) noexcept
    -> std::unique_ptr<SkeletonServiceElementBinding>
{
    static_assert((element_type == ServiceElementType::EVENT) || (element_type == ServiceElementType::FIELD));

    const InstanceIdentifierView identifier_view{identifier};

    using ReturnType = std::unique_ptr<SkeletonServiceElementBinding>;
    auto visitor = score::cpp::overload(
        [identifier_view, &parent, &service_element_name, &meta_info](
            const LolaServiceTypeDeployment& lola_service_type_deployment) -> ReturnType {
            auto* const lola_parent = dynamic_cast<lola::Skeleton*>(&SkeletonBaseView{parent}.GetBinding());
            if (lola_parent == nullptr)
            {
                score::mw::log::LogFatal("lola") << "Skeleton service element could not be created because parent "
                                                    "skeleton binding is a nullptr.";
                return nullptr;
            }

            const auto& service_instance_deployment = identifier_view.GetServiceInstanceDeployment();
            const auto& lola_service_instance_deployment =
                GetServiceInstanceDeploymentBinding<LolaServiceInstanceDeployment>(service_instance_deployment);

            const auto& lola_service_element_instance_deployment = GetServiceElementInstanceDeployment<element_type>(
                lola_service_instance_deployment, std::string{service_element_name});

            // TODO: Pass in the FieldAbilities when GenericSkeletonFields are implemented.
            const lola::SkeletonEventProperties skeleton_event_properties = detail::CreateSkeletonEventProperties(
                lola_service_element_instance_deployment, std::optional<FieldTagsStore>{});

            const auto lola_service_element_id =
                GetServiceElementId<element_type>(lola_service_type_deployment, std::string{service_element_name});
            const lola::ElementFqId element_fq_id{lola_service_type_deployment.service_id_,
                                                  lola_service_element_id,
                                                  lola_service_instance_deployment.instance_id_.value().GetId(),
                                                  element_type};

            return std::make_unique<SkeletonServiceElement>(*lola_parent,
                                                            service_element_name,
                                                            skeleton_event_properties,
                                                            element_fq_id,
                                                            meta_info,
                                                            tracing::SkeletonEventTracingData{});
        },
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return nullptr;
        });

    return std::visit(visitor, identifier_view.GetServiceTypeDeployment().binding_info_);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_SERVICE_ELEMENT_BINDING_FACTORY_IMPL_H
