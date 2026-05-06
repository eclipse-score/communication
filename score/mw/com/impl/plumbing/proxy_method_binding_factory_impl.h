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
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_method.h"
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/plumbing/i_proxy_method_binding_factory.h"
#include "score/mw/com/impl/plumbing/lola_proxy_element_building_blocks.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include "score/memory/data_type_size_info.h"
#include "score/mw/log/logging.h"

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

LolaMethodInstanceDeployment::QueueSize GetQueueSize(HandleType parent_handle,
                                                     const std::string& method_name_str,
                                                     MethodType method_type);

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
                                               const std::string_view method_name,
                                               MethodType method_type) noexcept override;
};

template <typename ReturnType, typename... ArgTypes>
lola::TypeErasedCallQueue::TypeErasedElementInfo GetTypeErasedElementInfo(HandleType parent_handle,
                                                                          const std::string& method_name_str,
                                                                          MethodType method_type)
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

    const LolaMethodInstanceDeployment::QueueSize queue_size =
        GetQueueSize(parent_handle, method_name_str, method_type);

    return lola::TypeErasedCallQueue::TypeErasedElementInfo{in_arg_type_info, return_type_info, queue_size};
}

template <typename ReturnType, typename... ArgTypes>
std::unique_ptr<ProxyMethodBinding> ProxyMethodBindingFactoryImpl<ReturnType(ArgTypes...)>::Create(
    HandleType parent_handle,
    ProxyBinding* parent_binding,
    const std::string_view method_name,
    MethodType method_type) noexcept
{
    // When method_type is Get or Set, method_name holds a field name, not a method name. A field
    // only has one id at the lola level, so the Get and the Set of the same field both resolve to
    // the same element id here. The Get-vs-Set split is added further down when we pair the id
    // with method_type to build the UniqueMethodIdentifier.
    const auto element_type = (method_type == MethodType::kGet || method_type == MethodType::kSet)
                                  ? ServiceElementType::FIELD
                                  : ServiceElementType::METHOD;

    const auto lookup = LookupLolaProxyElement(parent_handle, parent_binding, method_name, element_type);
    if (!lookup.has_value())
    {
        score::mw::log::LogError("lola")
            << "Proxy Method binding could not be created for" << method_name
            << "because the parent proxy binding is not a lola binding or the element could not be resolved.";
        return nullptr;
    }

    const auto method_name_str = std::string{method_name};
    const lola::TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info =
        GetTypeErasedElementInfo<ReturnType, ArgTypes...>(parent_handle, method_name_str, method_type);

    // Pairing the id with method_type is what keeps Get and Set separate from here on. They share
    // the same id, but the pair (id, method_type) is different, so the two end up in separate
    // entries of the proxy_methods_ / skeleton_methods_ maps in the binding layer.
    const lola::ProxyMethodInstanceIdentifier proxy_method_instance_identifier{
        lookup->parent.GetProxyInstanceIdentifier(), {lookup->element_fq_id.element_id_, method_type}};

    return std::make_unique<lola::ProxyMethod>(
        lookup->parent, proxy_method_instance_identifier, type_erased_element_info);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_IMPL_H
