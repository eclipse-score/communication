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

#include "score/mw/com/gateway/gateway_application/gateway_error.h"
#include "score/mw/com/gateway/transport_layer/transport.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/types.h"
#include "score/result/result.h"

#include <string>
#include <vector>

namespace score::mw::com::gateway
{

/// \brief Pairs an allocated sample slot with the service element it was allocated for.
/// \details Returned by GatewayCore::AllocateSamples() and handed back (with the payload copied in) to
/// GatewayCore::SendOrUpdateSamples(), so a single bulk call can cover multiple elements.
struct AllocatedSample
{
    /// \brief name of the service element (event/field) this sample slot was allocated for.
    std::string element_name;
    /// \brief allocated (but not-yet-filled, until handed to SendOrUpdateSamples()) sample slot, obtained from the
    /// local (generic) skeleton representing the forwarded service instance.
    score::mw::com::SampleAllocateePtr<void> sample;
};

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
    virtual score::Result<void> ProvideService(score::mw::com::InstanceSpecifier service_instance_specifier,
                                               std::vector<score::mw::com::EventInfo> service_elements) = 0;

    /// \brief Stop providing the given service instance locally within the destination domain.
    /// \param service_instance_specifier instance specifier of the service instance to stop offering. It is expected,
    /// that this specifier is configured/existent in the mw_com_config.json at the local gateway side.
    virtual void StopOfferService(score::mw::com::InstanceSpecifier service_instance_specifier) = 0;

    /// \brief Offer the given service instance locally within the destination domain.
    /// \param service_instance_specifier instance specifier of the service instance to offer. It is expected, that
    /// this specifier is configured/existent in the mw_com_config.json at the local gateway side.
    /// \return result indicating success or failure.
    virtual score::Result<void> OfferService(score::mw::com::InstanceSpecifier service_instance_specifier) = 0;

    /// \brief Register an event-update notification for the given service instance and element locally within the
    /// destination domain.
    /// \param service_instance_specifier instance specifier of the service instance, for which registration takes
    /// place. It is expected, that this specifier is configured/existent in the mw_com_config.json at the local
    /// gateway side.
    /// \param element_type type of the service element (event, field, method). Currently only EVENT is supported.
    /// \param element_name name of the service element for which update notifications shall be registered.
    /// \return result indicating success or failure.
    virtual score::Result<void> RegisterUpdateNotification(score::mw::com::InstanceSpecifier service_instance_specifier,
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
    virtual score::Result<void> UnregisterUpdateNotification(
        score::mw::com::InstanceSpecifier service_instance_specifier,
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
    virtual score::Result<void> NotifyUpdate(score::mw::com::InstanceSpecifier service_instance_specifier,
                                             impl::ServiceElementType updated_element_type,
                                             std::string updated_element_name) = 0;

    /// \brief Allocates sample slots for incoming sample data updates, required only when
    /// IsMemorySharingSupported() == false for the involved transport layer.
    /// \details This API is expected to be called by the transport layer implementation on the service-receiving
    /// side, before it copies received sample bytes (see Transport::ForwardSamples()) into the allocated slots and
    /// hands them back via SendOrUpdateSamples(). Internally this is expected to delegate to the Allocate() API of
    /// the (generic) skeleton event/field representing the given service element locally.
    /// \param service_instance_specifier instance specifier of the service instance owning the elements to allocate
    /// for. It is expected, that this specifier is configured/existent in the mw_com_config.json at the local
    /// gateway side.
    /// \param element_names names of the service elements (currently only EVENT is supported), for which a sample
    /// slot needs to be allocated. One slot is allocated per entry, preserving order.
    /// \return collection of allocated sample slots, one per entry in element_names, or an error.
    /// \note Default implementation returns GatewayErrorc::kNotSupported. Only relevant for transports without
    /// memory sharing support.
    virtual score::Result<std::vector<AllocatedSample>> AllocateSamples(
        score::mw::com::InstanceSpecifier service_instance_specifier,
        std::vector<std::string> element_names)
    {
        static_cast<void>(service_instance_specifier);
        static_cast<void>(element_names);
        return score::MakeUnexpected(GatewayErrorc::kNotSupported);
    }

    /// \brief Sends/updates the given, previously allocated (see AllocateSamples()) and now filled-in sample slots,
    /// required only when IsMemorySharingSupported() == false for the involved transport layer.
    /// \details This API is expected to be called by the transport layer implementation on the service-receiving
    /// side, after it has copied the received sample bytes into the slots obtained from AllocateSamples(). Internally
    /// this is expected to delegate to the Send() API of the (generic) skeleton event/field representing the given
    /// service element locally.
    /// \param service_instance_specifier instance specifier of the service instance owning the elements to update.
    /// \param samples collection of allocated sample slots, now containing the updated sample data to be sent.
    /// \return result indicating success or failure.
    /// \note Default implementation returns GatewayErrorc::kNotSupported. Only relevant for transports without
    /// memory sharing support.
    virtual score::Result<void> SendOrUpdateSamples(score::mw::com::InstanceSpecifier service_instance_specifier,
                                                    std::vector<AllocatedSample> samples)
    {
        static_cast<void>(service_instance_specifier);
        static_cast<void>(samples);
        return score::MakeUnexpected(GatewayErrorc::kNotSupported);
    }

    /// \brief Registers a subscription for an event/field, originating from the service-consuming domain, required
    /// only when IsMemorySharingSupported() == false for the involved transport layer.
    /// \details This API is expected to be called by the transport layer implementation on the service-forwarding
    /// side, when the destination gateway signals (via Transport::Subscribe()) that it now has a local subscriber for
    /// the given service element. It is expected to trigger a Subscribe() on the (generic) proxy event/field
    /// representing the given service element locally.
    /// \param service_instance_specifier instance specifier of the service instance owning the service element.
    /// \param element_type type of the service element (event, field, method). Currently only EVENT is supported.
    /// \param element_name name of the service element to subscribe to.
    /// \return result indicating success or failure.
    /// \note Default implementation returns GatewayErrorc::kNotSupported. Only relevant for transports without
    /// memory sharing support.
    virtual score::Result<void> Subscribe(score::mw::com::InstanceSpecifier service_instance_specifier,
                                          impl::ServiceElementType element_type,
                                          std::string element_name)
    {
        static_cast<void>(service_instance_specifier);
        static_cast<void>(element_type);
        static_cast<void>(element_name);
        return score::MakeUnexpected(GatewayErrorc::kNotSupported);
    }

    /// \brief Unregisters a subscription for an event/field. See Subscribe() for the corresponding subscription
    /// semantics; required only when IsMemorySharingSupported() == false for the involved transport layer.
    /// \param service_instance_specifier instance specifier of the service instance owning the service element.
    /// \param element_type type of the service element (event, field, method). Currently only EVENT is supported.
    /// \param element_name name of the service element to unsubscribe from.
    /// \return result indicating success or failure.
    /// \note Default implementation returns GatewayErrorc::kNotSupported. Only relevant for transports without
    /// memory sharing support.
    virtual score::Result<void> Unsubscribe(score::mw::com::InstanceSpecifier service_instance_specifier,
                                            impl::ServiceElementType element_type,
                                            std::string element_name)
    {
        static_cast<void>(service_instance_specifier);
        static_cast<void>(element_type);
        static_cast<void>(element_name);
        return score::MakeUnexpected(GatewayErrorc::kNotSupported);
    }
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_CORE_H
