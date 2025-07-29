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
#ifndef SCORE_MW_COM_IMPL_PROXY_BASE_H
#define SCORE_MW_COM_IMPL_PROXY_BASE_H

#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/proxy_event_binding.h"

#include "score/memory/string_literal.h"
#include "score/result/result.h"

#include <score/span.hpp>

#include <memory>

namespace score::mw::com::impl
{

/// \brief Base class for all binding-unspecific proxies that are generated from the IDL.
class ProxyBase
{

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a view to the private members of this class.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyBaseView;

  public:
    /// \brief Identifier for a specific instance of a specific service
    using HandleType = ::score::mw::com::impl::HandleType;
    using EventNameList = score::cpp::v1::span<const score::StringLiteral>;

    /// \brief Creation of ProxyBase which should be called by parent class (generated Proxy or GenericProxy)
    ProxyBase(std::unique_ptr<ProxyBinding> proxy_binding, HandleType handle);

    virtual ~ProxyBase() = default;

    /// \brief Tries to find a service that matches the given specifier synchronously.
    /// \details Does a synchronous one-shot lookup/find, which service instance(s) matching the specifier are there.
    ///
    /// \requirement SWS_CM_00622
    ///
    /// \param specifier The instance specifier of the service.
    /// \return A result which on success contains a list of found handles that can be used to create a proxy. On
    /// failure, returns an error code.
    static Result<ServiceHandleContainer<HandleType>> FindService(InstanceSpecifier specifier) noexcept;

    /// \brief Tries to find a service that matches the given instance identifier synchronously.
    /// \details Does a synchronous one-shot lookup/find, which service instance(s) matching the specifier are there.
    ///
    /// \param specifier The instance_identifier of the service.
    /// \return A result which on success contains a list of found handles that can be used to create a proxy. On
    /// failure, returns an error code.
    static Result<ServiceHandleContainer<HandleType>> FindService(InstanceIdentifier instance_identifier) noexcept;

    static Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType> handler,
                                                      InstanceIdentifier instance_identifier) noexcept;

    static Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType> handler,
                                                      InstanceSpecifier instance_specifier) noexcept;

    static score::ResultBlank StopFindService(const FindServiceHandle handle) noexcept;

    /// Returns the handle that was used to instantiate this proxy.
    ///
    /// \return Handle identifying the service that this proxy is connected to.
    const HandleType& GetHandle() const noexcept;

  protected:
    /// \brief A Proxy shall not be copyable
    /// \requirement SWS_CM_00136
    ProxyBase(const ProxyBase&) = delete;
    ProxyBase& operator=(const ProxyBase&) = delete;

    /// \brief A Proxy shall be movable
    /// \requirement SWS_CM_00137
    ProxyBase(ProxyBase&& other) = default;
    ProxyBase& operator=(ProxyBase&& other) = default;

    bool AreBindingsValid() const noexcept
    {
        const bool is_proxy_binding_valid{proxy_binding_ != nullptr};
        return is_proxy_binding_valid && are_service_element_bindings_valid_;
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to exchange this information between the ProxyBase and the
    // GenericProxy. There are no class invariants to maintain which could be violated by directly accessing these
    // member variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::unique_ptr<ProxyBinding> proxy_binding_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    HandleType handle_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool are_service_element_bindings_valid_;
};

class ProxyBaseView final
{
  public:
    /// \brief Create a view on the ProxyBase instance to allow for additional methods on the ProxyBase.
    ///
    /// \param proxy_base The ProxyBase to create the view on.
    explicit ProxyBaseView(ProxyBase& proxy_base) noexcept;

    /// \brief Return a reference to the underlying implementation provided by the binding.
    ///
    /// \return Pointer to the proxy binding.
    ProxyBinding* GetBinding() noexcept;

    const HandleType& GetAssociatedHandleType() const noexcept
    {
        return proxy_base_.handle_;
    }

    void MarkServiceElementBindingInvalid() noexcept
    {
        proxy_base_.are_service_element_bindings_valid_ = false;
    }

  private:
    ProxyBase& proxy_base_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_BASE_H
