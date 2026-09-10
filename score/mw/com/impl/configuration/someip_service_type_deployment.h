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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_TYPE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_TYPE_DEPLOYMENT_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/configuration/binding_service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_element_id.h"
#include "score/mw/com/impl/configuration/someip_service_id.h"

namespace score::mw::com::impl
{

using SomeIpEventId = SomeIpServiceElementId;
using SomeIpFieldId = SomeIpServiceElementId;
using SomeIpMethodId = SomeIpServiceElementId;

/// \brief Type (i.e. service interface) level deployment of the SOME/IP binding.
///
/// Holds the wire ids which are identical for every instance of the service interface. The BindingType::kSomeIp tag
/// makes this a distinct type from LolaServiceTypeDeployment, which is required so that both can be held as separate
/// alternatives of ServiceTypeDeployment::BindingInformation.
using SomeIpServiceTypeDeployment =
    BindingServiceTypeDeployment<SomeIpEventId, SomeIpFieldId, SomeIpMethodId, SomeIpServiceId, BindingType::kSomeIp>;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_TYPE_DEPLOYMENT_H
