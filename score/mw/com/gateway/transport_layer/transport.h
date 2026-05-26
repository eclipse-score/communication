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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_H
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_H

#include "score/mw/com/impl/generic_skeleton.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/result/result.h"

#include <string>
#include <vector>

namespace score::mw::com::gateway
{

/// \brief Abstract base class for gateway transport layer implementations.
class Transport
{
  public:
    Transport() = default;
    virtual ~Transport() = default;

    /// \brief Returns whether this transport implementation supports memory sharing between source and destination.
    /// \return true if shared memory transport is supported, false otherwise.
    virtual bool IsMemorySharingSupported() const = 0;

    /// \brief Sets up the transport layer (e.g. establishes connections or allocates resources).
    /// \return result indicating success or failure.
    virtual score::Result<void> Setup() = 0;

    /// \brief Shuts down the transport layer and releases all resources.
    virtual void Shutdown() = 0;

    /// \brief ProvideService API signature to be provided when IsMemorySharingSupported() == true.
    /// \details Transport layer implementation shall "forward" this call to the destination gateway, which is
    /// expected to create a (generic) skeleton for the given service instance and service elements. The (generic)
    /// skeleton created by the destination gateway in this case expects to work on existing shared memory, which is
    /// shared between the source and destination gateway. The transport layer implementation must ensure, that the
    /// shared memory is visible/accessible on the destination gateway side, when the (generic) skeleton gets created.
    /// \param service_instance_specifier instance specifier of the service to provide. It is expected, that this
    /// specifier is configured/existent in the mw_com_config.json at the destination gateway side.
    /// \param service_elements configuration of the service elements (events, fields, methods) of the service to
    /// provide. This information is needed to create the (generic) skeleton on the destination gateway side.
    /// \return result indicating success or failure.
    virtual score::Result<void> ProvideService(impl::InstanceSpecifier service_instance_specifier,
                                               std::vector<impl::EventInfo> service_elements) = 0;

    /// \brief OfferService API to trigger service-instance offering at the destination gateway side.
    /// \details Transport layer implementation shall "forward" this call to the destination gateway and trigger the
    /// offering of the service instance at the destination gateway side. The service instance is expected to be
    /// already created at the destination gateway side, e.g. by a previous call to ProvideService().
    /// \param service_instance_specifier instance specifier of the service instance to offer.
    /// \return result indicating success or failure.
    virtual score::Result<void> OfferService(impl::InstanceSpecifier service_instance_specifier) = 0;

    /// \brief StopOfferService API to trigger service-instance stop-offer at the destination gateway side.
    /// \details Transport layer implementation shall "forward" this call to the destination gateway and trigger the
    /// stop-offer of the service instance at the destination gateway side. The service instance is expected to be
    /// already created at the destination gateway side, e.g. by a previous call to ProvideService().
    /// \param service_instance_specifier instance specifier of the service instance to stop-offer.
    /// \return result indicating success or failure.
    virtual score::Result<void> StopOfferService(impl::InstanceSpecifier service_instance_specifier) = 0;

    /// \brief NotifyUpdate API to inform the destination gateway about updates of service elements of a service
    /// instance on the source gateway side.
    /// \details The transport layer implementation shall "forward" this call to the destination gateway, which is
    /// expected to trigger update notifications at the (generic) skeleton of the related service instance on the
    /// destination gateway side for the given service element. API will only be called by the source gateway, if
    /// previously the destination gateway registered for update notifications for the given service instance and
    /// service element by calling the RegisterUpdateNotification API.
    /// \param service_instance_specifier instance specifier of the service instance owning the service element, for
    /// which an update shall be notified.
    /// \param updated_element_type type of the updated service element (event, field, method). Currently only EVENT
    /// is supported.
    /// \param updated_element_name name of the updated service element.
    /// \return result indicating success or failure.
    virtual score::Result<void> NotifyUpdate(impl::InstanceSpecifier service_instance_specifier,
                                             impl::ServiceElementType updated_element_type,
                                             std::string updated_element_name) = 0;

    /// \brief API to register for update notifications for service elements of a service instance at the source
    /// gateway side.
    /// \details The transport layer implementation shall "forward" this call to the source gateway, which is expected
    /// to register for update notifications at the (generic) proxy of the related service instance on the source
    /// gateway side for the given service element.
    /// \param service_instance_specifier instance specifier of the service instance owning the service element, for
    /// which an update-notification shall be registered.
    /// \param element_type type of the service element (event, field, method). Currently only EVENT is supported.
    /// \param element_name name of the service element.
    /// \return result indicating success or failure.
    virtual score::Result<void> RegisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                                           impl::ServiceElementType element_type,
                                                           std::string element_name) = 0;

    /// \brief API to unregister update notifications for service elements of a service instance at the source gateway
    /// side.
    /// \details See RegisterUpdateNotification API for the corresponding registration semantics.
    /// \param service_instance_specifier instance specifier of the service instance owning the service element, for
    /// which the update-notification shall be unregistered.
    /// \param element_type type of the service element (event, field, method). Currently only EVENT is supported.
    /// \param element_name name of the service element.
    /// \return result indicating success or failure.
    virtual score::Result<void> UnregisterUpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                                                             impl::ServiceElementType element_type,
                                                             std::string element_name) = 0;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_H
