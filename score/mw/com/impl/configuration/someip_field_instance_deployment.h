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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_FIELD_INSTANCE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_FIELD_INSTANCE_DEPLOYMENT_H

#include "score/json/json_parser.h"
#include "score/mw/com/impl/configuration/someip_event_instance_deployment.h"

#include <cstdint>

namespace score::mw::com::impl
{

class SomeIpFieldInstanceDeployment
{
  public:
    explicit SomeIpFieldInstanceDeployment(SomeIpEventInstanceDeployment event_deployment,
                                           const bool use_get_if_available,
                                           const bool use_set_if_available) noexcept;

    explicit SomeIpFieldInstanceDeployment(const score::json::Object& json_object);

    static SomeIpFieldInstanceDeployment CreateFromJson(const score::json::Object& json_object);

    score::json::Object Serialize() const;

    // coverity[autosar_cpp14_m11_0_1_violation]
    SomeIpEventInstanceDeployment someip_event_instance_deployment_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool use_get_if_available_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool use_set_if_available_;

    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    constexpr static std::uint32_t serializationVersion = 1U;

    friend bool operator==(const SomeIpFieldInstanceDeployment& lhs,
                           const SomeIpFieldInstanceDeployment& rhs) noexcept;
};

bool operator==(const SomeIpFieldInstanceDeployment& lhs, const SomeIpFieldInstanceDeployment& rhs) noexcept;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_FIELD_INSTANCE_DEPLOYMENT_H
