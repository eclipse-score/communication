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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_INSTANCE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_INSTANCE_DEPLOYMENT_H

#include "score/mw/com/impl/configuration/someip_service_instance_id.h"

#include "score/json/json_parser.h"

#include <cstdint>
#include <optional>

namespace score::mw::com::impl
{

/// \brief Instance level deployment of the SOME/IP binding.
///
/// Holds everything that differs between two instances of the same service interface. In contrast to the LoLa binding
/// there are no shared memory sizes or slot counts here, since SOME/IP transports serialized messages over a socket
/// instead of shared memory.
class SomeIpServiceInstanceDeployment
{
  public:
    SomeIpServiceInstanceDeployment() = default;
    explicit SomeIpServiceInstanceDeployment(const score::json::Object& json_object);
    explicit SomeIpServiceInstanceDeployment(const std::optional<SomeIpServiceInstanceId>& instance_id) noexcept;

    score::json::Object Serialize() const;

    // Note the struct is not compliant to POD type containing non-POD member.
    // The struct is used as a config storage obtained by performing the parsing json object.
    // Public access is required by the implementation to reach the following members of the struct.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<SomeIpServiceInstanceId> instance_id_;

    constexpr static std::uint32_t serializationVersion{1U};
};

bool areCompatible(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept;
bool operator==(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept;
bool operator<(const SomeIpServiceInstanceDeployment& lhs, const SomeIpServiceInstanceDeployment& rhs) noexcept;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_SERVICE_INSTANCE_DEPLOYMENT_H
