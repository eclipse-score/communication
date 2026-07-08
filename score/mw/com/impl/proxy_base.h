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
#include "score/mw/com/impl/reference_to_moveable.h"

#include "score/result/result.h"

#include <score/span.hpp>

#include <memory>
#include <string_view>

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
    static score::Result<void> StopFindService(const FindServiceHandle handle) noexcept;

    /**
     * \api
     * \brief Returns the handle that was used to instantiate this proxy.
     * \return Handle identifying the service that this proxy is connected to.
     */
    const HandleType& GetHandle() const& noexcept;

  protected:
    using ProxyEvents =
        std::map<std::string_view, std::reference_wrapper<ReferenceToMoveable<ProxyEventBase>::Reference>>;
    using ProxyFields =
        std::map<std::string_view, std::reference_wrapper<ReferenceToMoveable<ProxyFieldBase>::Reference>>;
    using ProxyMethods =
        std::map<std::string_view, std::reference_wrapper<ReferenceToMoveable<ProxyMethodBase>::Reference>>;

    /// \brief A Proxy shall not be copyable
    /// \requirement SWS_CM_00136
    ProxyBase(const ProxyBase&) = delete;
    ProxyBase& operator=(const ProxyBase&) = delete;

    /// \brief A Proxy shall be movable
    /// \requirement SWS_CM_00137
    ProxyBase(ProxyBase&& other) noexcept = default;
    ProxyBase& operator=(ProxyBase&& other) noexcept = default;

    /// \brief Deinitialize all events and fields and tears down binding-level state.
    ///  Must only be invoked from the wrapper class destructor (ProxyWrapperClass / GenericProxy).
    ///      Calling this from user code leaves the binding in a torn-down state mid-life.
    void Deinitialize();

    bool AreBindingsValid() const noexcept;

    /// \brief Dispatches to the binding for any binding specific setup and then initializes all method InArgs and
    /// Return values.
    ///
    /// The initialization of the InArgs and Return values is done on the binding independent level since the binding
    /// level is type erased. The values are initialized once on startup and then reused (i.e. we never re-initialize
    /// the values when calling a method).
    ///
    /// \param additional_shm_size_bytes Additional shared memory size in bytes to be allocated for methods in addition
    /// to the size calculated for the size of the method in args and return values. This is a temporary workaround
    /// added to allow using types which dynamically allocate memory once at runtime. This is not currently public and
    /// should not be used by user applications. (SWP-269486)
    Result<void> SetupMethods(std::size_t additional_shm_size_bytes);

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to exchange this information between the ProxyBase and the
    // GenericProxy. There are no class invariants to maintain which could be violated by directly accessing these
    // member variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::unique_ptr<ProxyBinding> proxy_binding_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    HandleType handle_;

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
    ProxyBinding& GetBinding() noexcept;

    const HandleType& GetAssociatedHandleType() const& noexcept;

    void RegisterEvent(const std::string_view event_name, ReferenceToMoveable<ProxyEventBase>::Reference& event);

    void RegisterField(const std::string_view field_name, ReferenceToMoveable<ProxyFieldBase>::Reference& field);

    void RegisterMethod(const std::string_view method_name, ReferenceToMoveable<ProxyMethodBase>::Reference& method);

    bool AreBindingsValid() const;

  private:
    ProxyBase& proxy_base_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_BASE_H
