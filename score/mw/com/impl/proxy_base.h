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

#include "score/result/result.h"

#include <score/span.hpp>

#include <memory>

namespace score::mw::com::impl
{

class ProxyEventBase;
class ProxyFieldBase;
class ProxyMethodBase;

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

    /// \brief Creation of ProxyBase which should be called by parent class (generated Proxy or GenericProxy)
    ProxyBase(std::unique_ptr<ProxyBinding> proxy_binding, HandleType handle);

    virtual ~ProxyBase() = default;

    /**
     * \api
     * \brief Tries to find a service that matches the given specifier synchronously.
     * \details Does a synchronous one-shot lookup/find, which service instance(s) matching the specifier are there.
     * \param specifier The instance specifier of the service.
     * \return A result which on success contains a list of found handles that can be used to create a proxy. On
     *         failure, returns an error code.
     * \requirement SWS_CM_00622
     */
    static Result<ServiceHandleContainer<HandleType>> FindService(InstanceSpecifier specifier) noexcept;

    /**
     * \api
     * \brief Tries to find a service that matches the given instance identifier synchronously.
     * \details Does a synchronous one-shot lookup/find, which service instance(s) matching the specifier are there.
     * \param instance_identifier The instance_identifier of the service.
     * \return A result which on success contains a list of found handles that can be used to create a proxy. On
     *         failure, returns an error code.
     */
    static Result<ServiceHandleContainer<HandleType>> FindService(InstanceIdentifier instance_identifier) noexcept;

    /**
     * \api
     * \brief Starts asynchronous service discovery that matches the given instance identifier.
     * \details Initiates a continuous service discovery operation. The provided handler will be called whenever
     *          matching service instances become available or unavailable.
     * \param handler The callback handler to be invoked when service availability changes.
     * \param instance_identifier The instance identifier of the service to find.
     * \return A result which on success contains a handle to control the find operation. On failure, returns an
     *         error code.
     */
    static Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType> handler,
                                                      InstanceIdentifier instance_identifier) noexcept;

    /**
     * \api
     * \brief Starts asynchronous service discovery that matches the given instance specifier.
     * \details Initiates a continuous service discovery operation. The provided handler will be called whenever
     *          matching service instances become available or unavailable.
     * \param handler The callback handler to be invoked when service availability changes.
     * \param instance_specifier The instance specifier of the service to find.
     * \return A result which on success contains a handle to control the find operation. On failure, returns an
     *         error code.
     */
    static Result<FindServiceHandle> StartFindService(FindServiceHandler<HandleType> handler,
                                                      InstanceSpecifier instance_specifier) noexcept;

    /**
     * \api
     * \brief Stops an ongoing asynchronous service discovery operation.
     * \details Terminates the service discovery initiated by StartFindService. After this call, the associated
     *          handler will no longer be invoked.
     * \param handle The handle returned by StartFindService identifying the find operation to stop.
     * \return A result indicating success or failure of stopping the find operation.
     */
    static score::ResultBlank StopFindService(const FindServiceHandle handle) noexcept;

    /**
     * \api
     * \brief Returns the handle that was used to instantiate this proxy.
     * \return Handle identifying the service that this proxy is connected to.
     */
    const HandleType& GetHandle() const noexcept;

  protected:
    using ProxyEvents = std::map<std::string_view, std::reference_wrapper<ProxyEventBase>>;
    using ProxyFields = std::map<std::string_view, std::reference_wrapper<ProxyFieldBase>>;
    using ProxyMethods = std::map<std::string_view, std::reference_wrapper<ProxyMethodBase>>;

    /// \brief A Proxy shall not be copyable
    /// \requirement SWS_CM_00136
    ProxyBase(const ProxyBase&) = delete;
    ProxyBase& operator=(const ProxyBase&) = delete;

    /// \brief A Proxy shall be movable
    /// \requirement SWS_CM_00137
    ProxyBase(ProxyBase&& other) noexcept;
    ProxyBase& operator=(ProxyBase&& other) noexcept;

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

    ProxyEvents events_;
    ProxyFields fields_;
    ProxyMethods methods_;
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

    void RegisterEvent(const std::string_view event_name, ProxyEventBase& event)
    {
        const auto result = proxy_base_.events_.emplace(event_name, event);
        const bool was_event_inserted = result.second;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_event_inserted, "Event cannot be registered as it already exists.");
    }

    void RegisterField(const std::string_view field_name, ProxyFieldBase& field)
    {
        const auto result = proxy_base_.fields_.emplace(field_name, field);
        const bool was_field_inserted = result.second;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_field_inserted, "Field cannot be registered as it already exists.");
    }

    void RegisterMethod(const std::string_view method_name, ProxyMethodBase& method)
    {
        const auto result = proxy_base_.methods_.emplace(method_name, method);
        const bool was_method_inserted = result.second;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_method_inserted, "Method cannot be registered as it already exists.");
    }

    void UpdateEvent(const std::string_view event_name, ProxyEventBase& event)
    {
        auto event_it = proxy_base_.events_.find(event_name);
        if (event_it == proxy_base_.events_.cend())
        {
            score::mw::log::LogError("lola")
                << "ProxyBaseView::UpdateEvent failed to update, because the requested event " << event_name
                << " doesn't exist!";
            std::terminate();
        }

        event_it->second = event;
    }

    void UpdateField(const std::string_view field_name, ProxyFieldBase& field)
    {
        auto field_it = proxy_base_.fields_.find(field_name);
        if (field_it == proxy_base_.fields_.cend())
        {
            score::mw::log::LogError("lola")
                << "ProxyBaseView::UpdateField failed to update, because the requested field " << field_name
                << " doesn't exist";
            std::terminate();
        }

        field_it->second = field;
    }

    void UpdateMethod(const std::string_view method_name, ProxyMethodBase& method)
    {
        auto method_it = proxy_base_.methods_.find(method_name);
        if (method_it == proxy_base_.methods_.cend())
        {
            score::mw::log::LogError("lola")
                << "ProxyBaseView::UpdateMethod failed to update, because the requested method " << method_name
                << " doesn't exist";
            std::terminate();
        }

        method_it->second = method;
    }

  private:
    ProxyBase& proxy_base_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_BASE_H
