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
#include "score/mw/com/impl/configuration/someip_field_instance_deployment.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"
#include "score/mw/com/impl/configuration/someip_event_instance_deployment.h"

#include <utility>

namespace score::mw::com::impl
{

SomeIpFieldInstanceDeployment::SomeIpFieldInstanceDeployment(SomeIpEventInstanceDeployment event_deployment) noexcept
    : someip_event_instance_deployment_{std::move(event_deployment)}
{
}

SomeIpFieldInstanceDeployment::SomeIpFieldInstanceDeployment(const score::json::Object& json_object)
    : SomeIpFieldInstanceDeployment(SomeIpFieldInstanceDeployment::CreateFromJson(json_object))
{
}

SomeIpFieldInstanceDeployment SomeIpFieldInstanceDeployment::CreateFromJson(const score::json::Object& json_object)
{
    // Delegate event-specific parsing to SomeIpEventInstanceDeployment (which also checks serialization version).
    auto event_deployment = SomeIpEventInstanceDeployment::CreateFromJson(json_object);

    return SomeIpFieldInstanceDeployment(std::move(event_deployment));
}

score::json::Object SomeIpFieldInstanceDeployment::Serialize() const
{
    auto json_object = someip_event_instance_deployment_.Serialize();

    return json_object;
}

bool operator==(const SomeIpFieldInstanceDeployment& lhs, const SomeIpFieldInstanceDeployment& rhs) noexcept
{
    // Adding brackets to the expression does not give additional value since only one logical operator is used which
    // is independent of the execution order
    // coverity[autosar_cpp14_a5_2_6_violation]
    return ((lhs.someip_event_instance_deployment_ == rhs.someip_event_instance_deployment_));
}

}  // namespace score::mw::com::impl
