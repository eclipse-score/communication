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
///
/// @file
/// @copyright Copyright (C) 2023, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
///

#ifndef SCORE_MW_COM_IMPL_GENERIC_PROXY_H
#define SCORE_MW_COM_IMPL_GENERIC_PROXY_H

#include "score/mw/com/impl/generic_proxy_event.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/service_element_map.h"

#include "score/result/result.h"

#include <memory>

namespace score::mw::com::impl
{
namespace test
{
class GenericProxyAttorney;
}

/// \todo The EventMap, events, needs to be populated with actual GenericProxyEvents.
// Suppress "AUTOSAR C++14 A12-1-6" rule findings. This rule states: "Derived classes that do not need further
// explicit initialization and require all the constructors from the base class shall use inheriting constructors.
// As per requirement 14005999 constructor shall be private.
// coverity[autosar_cpp14_a12_1_6_violation]
/**
 * \api
 * \brief GenericProxy is a non-binding specific Proxy class which doesn't require any type information for its events.
 * This means that it can connect to a service providing instance (skeleton) just based on deployment information
 * specified at runtime.
 * \details It contains a map of events which can access strongly-typed events in a type-erased way i.e. by accessing a
 * raw memory buffer. It is the generic analogue of a Proxy, which contains strongly-typed events. While the Proxy is
 * usually generated from the IDL, a GenericProxy can be manually instantiated at runtime based on deployment
 * information.
 */
class GenericProxy : public ProxyBase
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design dessision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class test::GenericProxyAttorney;

  public:
    using EventMap = ServiceElementMap<GenericProxyEvent>;

    /**
     * \api
     * \brief Exception-less GenericProxy constructor
     * \param instance_handle Handle to the instance
     * \return Result containing the created GenericProxy instance or an error code.
     */
    static Result<GenericProxy> Create(HandleType instance_handle) noexcept;

    /**
     * \api
     * \brief Returns a reference to the event map.
     * \return Reference to the event map.
     */
    EventMap& GetEvents() noexcept;

  private:
    GenericProxy(std::unique_ptr<ProxyBinding> proxy_binding, HandleType instance_handle);

    void FillEventMap(const std::vector<std::string_view>& event_names) noexcept;

    EventMap events_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_PROXY_H
