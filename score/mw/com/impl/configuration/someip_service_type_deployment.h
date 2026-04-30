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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_TYPE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_TYPE_DEPLOYMENT_H

#include "score/mw/com/impl/configuration/binding_service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_event_id.h"
#include "score/mw/com/impl/configuration/someip_field_id.h"
#include "score/mw/com/impl/configuration/someip_method_id.h"
#include "score/mw/com/impl/configuration/someip_service_id.h"

namespace score::mw::com::impl
{

// NOLINTBEGIN
// A distinct struct (not a type alias) is required so that SomeIpServiceTypeDeployment and LolaServiceTypeDeployment
// are different types in std::variant, even though both instantiate BindingServiceTypeDeployment with the same
// underlying integer types (all are uint16_t aliases).
struct SomeIpServiceTypeDeployment
    : BindingServiceTypeDeployment<SomeIpEventId, SomeIpFieldId, SomeIpMethodId, SomeIpServiceId>
{
    using BindingServiceTypeDeployment<SomeIpEventId, SomeIpFieldId, SomeIpMethodId, SomeIpServiceId>::
        BindingServiceTypeDeployment;
};
// NOLINTEND

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_TYPE_DEPLOYMENT_H
