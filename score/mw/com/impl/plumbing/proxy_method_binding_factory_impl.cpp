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
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory_impl.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_method.h"
#include "score/mw/com/impl/plumbing/binding_factory_error.h"
#include "score/mw/com/impl/plumbing/lola_proxy_element_building_blocks.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/overload.hpp>

namespace score::mw::com::impl::detail
{

bool IsMethodOrFieldEnabled(const LolaServiceInstanceDeployment& lola_service_instance_deployment,
                            const std::string& method_name_str,
                            MethodType method_type)
{
    if ((method_type == MethodType::kGet) || (method_type == MethodType::kSet))
    {
        const auto field_it = lola_service_instance_deployment.fields_.find(method_name_str);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(field_it != lola_service_instance_deployment.fields_.end(),
                                                    "Could not find field deployment information for field method");
        return (method_type == MethodType::kGet) ? field_it->second.use_get_if_available_
                                                 : field_it->second.use_set_if_available_;
    }
    const auto method_it = lola_service_instance_deployment.methods_.find(method_name_str);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(method_it != lola_service_instance_deployment.methods_.end(),
                                                "Could not find method deployment information for method");
    return method_it->second.enabled_;
}

LolaMethodInstanceDeployment::QueueSize GetQueueSize(const HandleType& parent_handle,
                                                     const std::string& method_name_str,
                                                     MethodType method_type)
{
    // Field Get/Set methods are synchronous and always use a fixed queue size of 1.
    // TODO: Revisit once field Get/Set deployment config grows its own queue-size entry.
    if ((method_type == MethodType::kGet) || (method_type == MethodType::kSet))
    {
        return 1U;
    }

    const auto& lola_service_instance_deployment = GetServiceInstanceDeploymentBinding<LolaServiceInstanceDeployment>(
        parent_handle.GetServiceInstanceDeployment());
    auto method_it = lola_service_instance_deployment.methods_.find(method_name_str);
    auto contains_method = method_it != lola_service_instance_deployment.methods_.end();
    if (!contains_method)
    {
        mw::log::LogFatal("lola") << "Provided a method name which can not be found in LolaServiceInstanceDeployment";
        // Reachable in principle, but must not be reached in production: A method name can be passed that is not part
        // of the deployment. In production, method names come from the generated proxy and are part of the deployment.
        SCORE_LANGUAGE_FUTURECPP_UNREACHABLE_MESSAGE(
            "Provided a method name which can not be found in LolaServiceInstanceDeployment");
    }
    const auto& lola_method_instance_deployment = method_it->second;

    auto queue_size_has_no_value = !lola_method_instance_deployment.queue_size_.has_value();
    if (queue_size_has_no_value)
    {
        mw::log::LogFatal("lola")
            << "ProxyMethod can not be created if queue_size is not configured on the proxy side.";
        // Reachable in principle, but must not be reached in production: A deployment without queue_size for a proxy
        // method is possible in principle. A deployment validated for production always configures it.
        SCORE_LANGUAGE_FUTURECPP_UNREACHABLE_MESSAGE(
            "ProxyMethod can not be created if queue_size is not configured on the proxy side.");
    }
    return lola_method_instance_deployment.queue_size_.value();
}

}  // namespace score::mw::com::impl::detail

namespace score::mw::com::impl
{

Result<std::unique_ptr<ProxyMethodBinding>> ProxyMethodBindingFactoryImpl::Create(
    HandleType parent_handle,
    ProxyBinding& parent_binding,
    const std::string_view method_name,
    MethodType method_type,
    std::optional<memory::DataTypeSizeInfo> in_arg_type_info,
    std::optional<memory::DataTypeSizeInfo> return_type_info)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(method_type != MethodType::kUnknown,
                                                "MethodType::kUnknown is not a valid method type");

    auto method_name_str = std::string{method_name};

    using LambdaReturnType = Result<std::unique_ptr<ProxyMethodBinding>>;

    auto deployment_info_visitor = score::cpp::overload(
        [&parent_handle, &parent_binding, &method_name_str, method_type, in_arg_type_info, return_type_info](
            const LolaServiceTypeDeployment& lola_type_deployment) -> LambdaReturnType {
            auto* const lola_proxy = dynamic_cast<lola::Proxy*>(&parent_binding);
            if (lola_proxy == nullptr)
            {
                score::mw::log::LogError("lola") << "Proxy Method binding could not be created for" << method_name_str
                                                 << "because the parent proxy binding is not a lola binding.";
                return MakeUnexpected(BindingFactoryErrorCode::kParentBindingIsNotLola);
            }

            const auto& service_instance_deployment = parent_handle.GetServiceInstanceDeployment();
            const auto& lola_service_instance_deployment =
                GetServiceInstanceDeploymentBinding<LolaServiceInstanceDeployment>(service_instance_deployment);

            if (!detail::IsMethodOrFieldEnabled(lola_service_instance_deployment, method_name_str, method_type))
            {
                score::mw::log::LogDebug("lola")
                    << "Proxy field or method " << method_name_str
                    << " was disabled in the instance deployment configuration. Not creating a binding for it.";
                return nullptr;
            }

            // When method_type is Get or Set, method_name holds a field name, not a method name. A field
            // only has one id at the lola level, so the Get and the Set of the same field both resolve to
            // the same element id here. The Get-vs-Set split is added further down when we pair the id
            // with method_type to build the UniqueMethodIdentifier.
            const auto service_element_type = ((method_type == MethodType::kGet) || (method_type == MethodType::kSet))
                                                  ? ServiceElementType::FIELD
                                                  : ServiceElementType::METHOD;

            const auto element_fq_id =
                GetElementFqId(parent_handle, lola_type_deployment, method_name_str, service_element_type);

            const auto queue_size = detail::GetQueueSize(parent_handle, method_name_str, method_type);
            lola::TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info{
                in_arg_type_info, return_type_info, queue_size};

            lola::ProxyMethodInstanceIdentifier proxy_method_instance_identifier{
                lola_proxy->GetProxyInstanceIdentifier(), {element_fq_id.element_id_, method_type}};

            return std::make_unique<lola::ProxyMethod>(
                *lola_proxy, proxy_method_instance_identifier, type_erased_element_info);
        },
        // The SOME/IP binding does not support this service element (yet). It is listed explicitly (instead of
        // being served by the score::cpp::blank arm) because std::visit requires an arm for every variant
        // alternative.
        [](const SomeIpServiceTypeDeployment&) noexcept -> LambdaReturnType {
            // TODO(someip-proxy-method): Create the SOME/IP proxy method binding.
            return MakeUnexpected(BindingFactoryErrorCode::kUnsupportedBindingType);
        },
        [](const score::cpp::blank&) noexcept -> LambdaReturnType {
            return MakeUnexpected(BindingFactoryErrorCode::kUnsupportedBindingType);
        });

    const auto& type_deployment = parent_handle.GetServiceTypeDeployment();
    return std::visit(deployment_info_visitor, type_deployment.binding_info_);
}

}  // namespace score::mw::com::impl
