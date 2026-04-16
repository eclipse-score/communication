/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/plumbing/generic_proxy_method_binding_factory.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/method_meta_info.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_method.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/overload.hpp>

#include <optional>
#include <string>
#include <variant>

namespace score::mw::com::impl
{

namespace
{

std::unique_ptr<ProxyMethodBinding> CreateForLola(HandleType parent_handle,
                                                  ProxyBinding* parent_binding,
                                                  const std::string_view method_name,
                                                  const LolaServiceTypeDeployment& lola_type_deployment) noexcept
{
    auto* const lola_parent = dynamic_cast<lola::Proxy*>(parent_binding);
    if (lola_parent == nullptr)
    {
        score::mw::log::LogError("lola") << "GenericProxyMethod binding could not be created for" << method_name
                                         << "because the parent proxy binding is not a lola binding.";
        return nullptr;
    }

    const auto method_name_str = std::string{method_name};
    const auto method_it = lola_type_deployment.methods_.find(method_name_str);
    if (method_it == lola_type_deployment.methods_.end())
    {
        score::mw::log::LogError("lola") << "GenericProxyMethod binding could not be created: method" << method_name
                                         << "is not part of the lola service type deployment.";
        return nullptr;
    }

    const auto instance_id = parent_handle.GetInstanceId();
    const auto* const lola_service_instance_id = std::get_if<LolaServiceInstanceId>(&(instance_id.binding_info_));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_service_instance_id != nullptr,
                                                "ServiceInstanceId does not contain lola binding.");
    const lola::ElementFqId element_fq_id{lola_type_deployment.service_id_,
                                          method_it->second,
                                          lola_service_instance_id->GetId(),
                                          ServiceElementType::METHOD};

    const auto meta = lola_parent->GetMethodMetaInfo(element_fq_id);
    if (!meta.has_value())
    {
        score::mw::log::LogError("lola")
            << "GenericProxyMethod binding could not be created: the skeleton has not published"
            << "meta info for method" << method_name;
        return nullptr;
    }

    // MethodMetaInfo carries impl::DataTypeMetaInfo; lola::ProxyMethod wants score::memory::DataTypeSizeInfo.
    const auto to_size_info =
        [](const std::optional<DataTypeMetaInfo>& data) -> std::optional<memory::DataTypeSizeInfo> {
        if (!data.has_value())
        {
            return std::nullopt;
        }
        return memory::DataTypeSizeInfo{data->size, data->alignment};
    };

    // Queue size is a proxy-side config, not carried via SHM. Read it from this proxy's deployment.
    const auto& lola_service_instance_deployment = GetServiceInstanceDeploymentBinding<LolaServiceInstanceDeployment>(
        parent_handle.GetServiceInstanceDeployment());
    const auto deployment_method_it = lola_service_instance_deployment.methods_.find(method_name_str);
    if (deployment_method_it == lola_service_instance_deployment.methods_.end() ||
        !deployment_method_it->second.queue_size_.has_value())
    {
        score::mw::log::LogError("lola")
            << "GenericProxyMethod binding could not be created: queue_size missing for method" << method_name;
        return nullptr;
    }
    const std::size_t queue_size = deployment_method_it->second.queue_size_.value();

    const lola::TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info{
        to_size_info(meta->in_args_type_info_), to_size_info(meta->return_type_info_), queue_size};

    const lola::ProxyMethodInstanceIdentifier proxy_method_instance_identifier{
        lola_parent->GetProxyInstanceIdentifier(), {element_fq_id.element_id_, MethodType::kMethod}};

    return std::make_unique<lola::ProxyMethod>(
        *lola_parent, proxy_method_instance_identifier, type_erased_element_info);
}

}  // namespace

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. std::visit may throw std::bad_variant_access only
// if the variant is valueless_by_exception, which cannot happen here because we do not throw
// exceptions during construction of the alternatives.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::unique_ptr<ProxyMethodBinding> GenericProxyMethodBindingFactory::Create(
    HandleType parent_handle,
    ProxyBinding* parent_binding,
    const std::string_view method_name) noexcept
{
    const auto& type_deployment = parent_handle.GetServiceTypeDeployment();
    auto visitor = score::cpp::overload(
        [parent_handle, parent_binding, method_name](
            const LolaServiceTypeDeployment& lola_deployment) -> std::unique_ptr<ProxyMethodBinding> {
            return CreateForLola(parent_handle, parent_binding, method_name, lola_deployment);
        },
        [](const score::cpp::blank&) noexcept -> std::unique_ptr<ProxyMethodBinding> {
            return nullptr;
        },
        [](const SomeIpServiceInstanceDeployment&) noexcept -> std::unique_ptr<ProxyMethodBinding> {
            return nullptr;
        });
    return std::visit(visitor, type_deployment.binding_info_);
}

}  // namespace score::mw::com::impl
