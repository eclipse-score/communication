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
#ifndef SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_CORE_H
#define SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_CORE_H

#include "score/mw/com/gateway/transport_layer/transport.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/result/result.h"

#include <string>
#include <vector>

namespace score::mw::com::gateway
{

class GatewayCore
{
  public:
    virtual ~GatewayCore() = default;

    /// \brief Provide the given service instance locally.
    /// \details This API is expected to be called by the transport layer implementation, when the transport layer
    /// receives a request from the source gateway to provide a service instance. It shall provide/create the given
    /// service instance locally within the destination domain by creating a Forwarding Skeleton (GenericSkeleton)
    /// and offer it.
    /// \param service_instance_specifier instance specifier of the service instance to provide. It is expected, that
    /// this specifier is configured/existent in the mw_com_config.json at the local gateway side.
    /// \param service_elements description of the service elements (events, fields, methods) which should be provided
    /// for the service instance, which will be realized as a Forwarding Skeleton (GenericSkeleton).
    /// \return result indicating success or failure.
    virtual score::Result<void> ProvideService(impl::InstanceSpecifier service_instance_specifier,
                                               std::vector<ServiceElementConfiguration> service_elements) = 0;

    /// \brief Stop providing the given service instance locally within the destination domain.
    /// \param service_instance_specifier instance specifier of the service instance to stop offering. It is expected,
    /// that this specifier is configured/existent in the mw_com_config.json at the local gateway side.
    virtual void StopOfferService(impl::InstanceSpecifier service_instance_specifier) = 0;

    /// \brief Offer the given service instance locally within the destination domain.
    /// \param service_instance_specifier instance specifier of the service instance to offer. It is expected, that
    /// this specifier is configured/existent in the mw_com_config.json at the local gateway side.
    /// \return result indicating success or failure.
    virtual score::Result<void> OfferService(impl::InstanceSpecifier service_instance_specifier) = 0;

    /// \brief Register an event-update notification for the given service instance and element locally within the
    /// destination domain.
    /// \param service_instance_specifier instance specifier of the service instance, for which registration takes
    /// place. It is expected, that this specifier is configured/existent in the mw_com_config.json at the local
    /// gateway side.
    /// \param element_type type of the service element (event, field, method). Currently only EVENT is supported.
    /// \param element_name name of the service element for which update notifications shall be registered.
    /// \return result indicating success or failure.
    virtual score::Result<void> RegisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                                           impl::ServiceElementType element_type,
                                                           std::string element_name) = 0;

    /// \brief Unregister an event-update notification for the given service instance and element locally within the
    /// destination domain.
    /// \param service_instance_specifier instance specifier of the service instance, for which unregistration takes
    /// place. It is expected, that this specifier is configured/existent in the mw_com_config.json at the local
    /// gateway side.
    /// \param element_type type of the service element (event, field, method). Currently only EVENT is supported.
    /// \param element_name name of the service element for which update notifications shall be unregistered.
    /// \return result indicating success or failure.
    virtual score::Result<void> UnregisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                                             impl::ServiceElementType element_type,
                                                             std::string element_name) = 0;

    /// \brief Notify an event update for the given service instance and element locally within the destination domain.
    /// \param service_instance_specifier instance specifier of the service instance, for which notification takes
    /// place. It is expected, that this specifier is configured/existent in the mw_com_config.json at the local
    /// gateway side.
    /// \param updated_element_type type of the element (currently only events supported) for which an update shall be
    /// notified.
    /// \param updated_element_name name of the element (e.g. event name) for which an update shall be notified.
    /// \return result indicating success or failure.
    virtual score::Result<void> NotifyUpdate(impl::InstanceSpecifier service_instance_specifier,
                                             impl::ServiceElementType updated_element_type,
                                             std::string updated_element_name) = 0;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_CORE_H
