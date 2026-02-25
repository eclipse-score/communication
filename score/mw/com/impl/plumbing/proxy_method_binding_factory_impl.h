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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_method.h"
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/plumbing/i_proxy_method_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_service_element_binding_factory_impl.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include "score/memory/data_type_size_info.h"
#include "score/mw/log/logging.h"

#include <score/blank.hpp>
#include <score/overload.hpp>

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

template <typename Signature>
class ProxyMethodBindingFactoryImpl : IProxyMethodBindingFactory
{
    static_assert(sizeof(Signature) == 0,
                  "ProxyMethodBindingFactory can only be instantiated with a function signature, e.g., "
                  "ProxyMethodBindingFactory<void(int, double)>. "
                  "You tried to use an unsupported signature type.");
};

LolaMethodInstanceDeployment::QueueSize GetQueueSize(HandleType parent_handle, const std::string& method_name_str);

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
template <typename ReturnType, typename... ArgTypes>
class ProxyMethodBindingFactoryImpl<ReturnType(ArgTypes...)> : public IProxyMethodBindingFactory
{
  public:
    /// Creates instances of the binding specific implementations for a proxy method with a particular data type.
    /// \tparam SampleType Type of the data that is exchanges
    /// \param handle The handle containing the binding information.
    /// \param method_name The binding unspecific name of the method inside the proxy denoted by handle.
    /// \return An instance of ProxyMethodBinding or nullptr in case of an error.
    std::unique_ptr<ProxyMethodBinding> Create(HandleType parent_handle,
                                               ProxyBinding* parent_binding,
                                               const std::string_view method_name) noexcept override;
};

template <typename ReturnType, typename... ArgTypes>
lola::TypeErasedCallQueue::TypeErasedElementInfo GetTypeErasedElementInfo(HandleType parent_handle,
                                                                          const std::string& method_name_str)
{
    std::optional<memory::DataTypeSizeInfo> in_arg_type_info{std::nullopt};
    std::optional<memory::DataTypeSizeInfo> return_type_info{std::nullopt};
    if constexpr (constexpr bool is_return_type_not_void = !std::is_same_v<ReturnType, void>; is_return_type_not_void)
    {
        return_type_info = CreateDataTypeSizeInfoFromTypes<ReturnType>();
    }
    if constexpr (constexpr bool is_inarg_pack_not_empty = !(sizeof...(ArgTypes) == 0); is_inarg_pack_not_empty)
    {
        in_arg_type_info = CreateDataTypeSizeInfoFromTypes<ArgTypes...>();
    }

    auto queue_size = GetQueueSize(parent_handle, method_name_str);
    return lola::TypeErasedCallQueue::TypeErasedElementInfo{in_arg_type_info, return_type_info, queue_size};
}

template <typename ReturnType, typename... ArgTypes>
std::unique_ptr<ProxyMethodBinding> ProxyMethodBindingFactoryImpl<ReturnType(ArgTypes...)>::Create(
    HandleType parent_handle,
    ProxyBinding* parent_binding,
    const std::string_view method_name) noexcept
{

    auto method_name_str = std::string{method_name};

    using LambdaReturnType = std::unique_ptr<ProxyMethodBinding>;
    auto lola_deployment_handler = [&parent_handle, parent_binding, &method_name_str](
                                       const LolaServiceTypeDeployment& lola_type_deployment) -> LambdaReturnType {
        auto* const lola_parent = dynamic_cast<lola::Proxy*>(parent_binding);
        if (parent_binding == nullptr)
        {
            score::mw::log::LogError("lola") << "Proxy Method could not be created because parent proxy "
                                              "binding is a nullptr.";
            return nullptr;
        }

        const auto instance_id = parent_handle.GetInstanceId();
        const auto* const lola_service_instance_id = std::get_if<LolaServiceInstanceId>(&(instance_id.binding_info_));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_service_instance_id != nullptr, "ServiceInstanceId does not contain lola binding.");

        constexpr auto element_type{ServiceElementType::METHOD};

        const auto lola_service_element_id = GetServiceElementId<element_type>(lola_type_deployment, method_name_str);
        const lola::ElementFqId element_fq_id{
            lola_type_deployment.service_id_, lola_service_element_id, lola_service_instance_id->GetId(), element_type};

        lola::TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info =
            GetTypeErasedElementInfo<ReturnType, ArgTypes...>(parent_handle, method_name_str);

        return std::make_unique<lola::ProxyMethod>(*lola_parent, element_fq_id, type_erased_element_info);
    };

    auto deployment_info_visitor = score::cpp::overload(
        lola_deployment_handler,
        [](const score::cpp::blank&) noexcept -> LambdaReturnType {
            return nullptr;
        },
        [](const SomeIpServiceInstanceDeployment&) noexcept -> LambdaReturnType {
            return nullptr;
        });

    const auto& type_deployment = parent_handle.GetServiceTypeDeployment();
    return std::visit(deployment_info_visitor, type_deployment.binding_info_);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_IMPL_H
