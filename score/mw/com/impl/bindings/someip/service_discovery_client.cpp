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
#include "score/mw/com/impl/bindings/someip/service_discovery_client.h"

#include "score/mw/com/impl/com_error.h"

#include <score/utility.hpp>

namespace score::mw::com::impl::someip
{

Result<void> ServiceDiscoveryClient::OfferService(const InstanceIdentifier instance_identifier)
{
    // TODO(someip-service-discovery): Implement offer service logic.
    // Service discovery has no provider-side work until SOME/IP transport support is integrated.
    score::cpp::ignore = instance_identifier;
    return {};
}

Result<void> ServiceDiscoveryClient::StopOfferService(
    const InstanceIdentifier instance_identifier,
    const IServiceDiscovery::QualityTypeSelector quality_type_selector)
{
    // TODO(someip-service-discovery): Implement stop offer service logic.
    // See OfferService(): no SOME/IP transport is currently available to withdraw service elements.
    score::cpp::ignore = instance_identifier;
    score::cpp::ignore = quality_type_selector;
    return {};
}

Result<void> ServiceDiscoveryClient::StartFindService(const FindServiceHandle find_service_handle,
                                                      FindServiceHandler<HandleType> handler,
                                                      const EnrichedInstanceIdentifier enriched_instance_identifier)
{
    score::cpp::ignore = find_service_handle;
    score::cpp::ignore = handler;
    score::cpp::ignore = enriched_instance_identifier;
    // TODO(someip-service-discovery): Implement start find service logic.
    return MakeUnexpected(ComErrc::kInvalidBindingInformation,
                          "StartFindService is not supported by the SOME/IP binding, as it has no proxy side yet");
}

Result<void> ServiceDiscoveryClient::StopFindService(const FindServiceHandle find_service_handle)
{
    score::cpp::ignore = find_service_handle;
    // TODO(someip-service-discovery): Implement stop find service logic.
    return MakeUnexpected(ComErrc::kInvalidBindingInformation,
                          "StopFindService is not supported by the SOME/IP binding, as it has no proxy side yet");
}

Result<ServiceHandleContainer<HandleType>> ServiceDiscoveryClient::FindService(
    const EnrichedInstanceIdentifier enriched_instance_identifier)
{
    score::cpp::ignore = enriched_instance_identifier;
    // TODO(someip-service-discovery): Implement find service logic.
    return MakeUnexpected(ComErrc::kInvalidBindingInformation,
                          "FindService is not supported by the SOME/IP binding, as it has no proxy side yet");
}

}  // namespace score::mw::com::impl::someip
