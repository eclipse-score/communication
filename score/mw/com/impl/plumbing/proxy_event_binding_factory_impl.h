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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/plumbing/binding_factory_error.h"
#include "score/mw/com/impl/plumbing/i_proxy_event_binding_factory.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/memory/data_type_size_info.h"

#include <memory>
#include <optional>
#include <string_view>

namespace score::mw::com::impl
{

namespace detail
{
/// \brief Creates a lola::ProxyEvent for the given deployment/handle/binding combination.
///
/// This encapsulates the logic shared by ProxyEventBindingFactoryImpl::Create() and
/// GenericProxyEventBindingFactoryImpl::Create(), which only differ in the concrete `ReturnType` they need
/// (i.e. whether the created lola::ProxyEvent is returned as a std::unique_ptr<ProxyEventBinding> or a
/// std::unique_ptr<lola::ProxyEvent>) and in whether they know the event's DataTypeSizeInfo up front.
/// \tparam ReturnType A score::Result<std::unique_ptr<T>> where lola::ProxyEvent is convertible to T.
/// \param parent_handle The handle containing the binding information.
/// \param parent_binding The parent proxy binding, which must be a lola::Proxy.
/// \param lola_type_deployment The lola specific service type deployment information.
/// \param event_or_field_name The binding unspecific name of the event/field inside the proxy denoted by handle.
/// \param service_element_type Whether the service element is an event or a field.
/// \param data_type_size_info Size/alignment info of the event's data type, if known by the caller (i.e. for a proxy
///        event with a known, non type-erased SampleType). If std::nullopt (i.e. for a generic/type-erased proxy
///        event), the DataTypeSizeInfo is instead looked up from the EventMetaInfo stored in shared-memory.
/// \return An instance of lola::ProxyEvent (wrapped in ReturnType) or an error in case binding creation fails.
template <typename ReturnType>
ReturnType CreateLolaProxyEvent(const HandleType& parent_handle,
                                ProxyBinding& parent_binding,
                                const LolaServiceTypeDeployment& lola_type_deployment,
                                const std::string_view event_or_field_name,
                                const ServiceElementType service_element_type,
                                const std::optional<memory::DataTypeSizeInfo> data_type_size_info);

}  // namespace detail

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
class ProxyEventBindingFactoryImpl : public IProxyEventBindingFactory
{
  public:
    /// Creates instances of the binding specific implementations for a proxy event with a particular data type.
    /// \param handle The handle containing the binding information.
    /// \param event_name The binding unspecific name of the event inside the proxy denoted by handle.
    /// \param data_type_size_info Size/alignment info of the event's data type.
    /// \return An instance of ProxyEventBinding or an error in case binding creation fails.
    Result<std::unique_ptr<ProxyEventBinding>> Create(HandleType parent_handle,
                                                      ProxyBinding& parent_binding,
                                                      const std::string_view event_name,
                                                      const ServiceElementType service_element_type,
                                                      memory::DataTypeSizeInfo data_type_size_info) noexcept override;
};

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
class GenericProxyEventBindingFactoryImpl : public IGenericProxyEventBindingFactory
{
  public:
    /// Creates instances of the binding specific implementations for a generic proxy event that has no data type.
    /// \param handle The handle containing the binding information.
    /// \param event_name The binding unspecific name of the event inside the proxy denoted by handle.
    /// \return An instance of ProxyEventBinding or an error in case binding creation fails.
    Result<std::unique_ptr<GenericProxyEventBinding>> Create(HandleType parent_handle,
                                                             ProxyBinding& parent_binding,
                                                             const std::string_view event_name,
                                                             const ServiceElementType service_element_type) override;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_EVENT_BINDING_FACTORY_IMPL_H
