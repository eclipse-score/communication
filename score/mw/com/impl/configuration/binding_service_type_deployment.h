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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_BINDING_SERVICE_TYPE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_BINDING_SERVICE_TYPE_DEPLOYMENT_H

#include "score/mw/com/impl/service_element_type.h"

#include "score/json/json_parser.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace score::mw::com::impl
{

template <typename EventIdType, typename FieldIdType, typename ServiceIdType>
class BindingServiceTypeDeployment
{
  public:
    using EventIdMapping = std::unordered_map<std::string, EventIdType>;
    using FieldIdMapping = std::unordered_map<std::string, FieldIdType>;
    using ServiceId = ServiceIdType;

    explicit BindingServiceTypeDeployment(const score::json::Object& json_object) noexcept;

    explicit BindingServiceTypeDeployment(const ServiceIdType service_id,
                                          EventIdMapping events = {},
                                          FieldIdMapping fields = {}) noexcept;

    json::Object Serialize() const noexcept;
    std::string_view ToHashString() const noexcept;

    // Note the struct is not compliant to POD type containing non-POD member.
    // The struct is used as a config storage obtained by performing the parsing json object.
    // Public access is required by the implementation to reach the following members of the struct.
    // coverity[autosar_cpp14_m11_0_1_violation]
    ServiceIdType service_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventIdMapping events_;  // key = event name
    // coverity[autosar_cpp14_m11_0_1_violation]
    FieldIdMapping fields_;  // key = field name

    /**
     * \brief The size of the hash string returned by ToHashString()
     *
     * The size is the amount of chars required to represent ServiceIdType as a hex string.
     */
    constexpr static std::size_t hashStringSize{2U * sizeof(ServiceIdType)};

    constexpr static std::uint32_t serializationVersion = 1U;

  private:
    /**
     * \brief Stringified format of this BindingServiceTypeDeployment which can be used for hashing.
     *
     * The hash is only based on service_id_
     */
    std::string hash_string_;
};

template <ServiceElementType service_element_type, typename EventIdType, typename FieldIdType, typename ServiceIdType>
auto GetServiceElementId(
    const BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>& binding_service_type_deployment,
    const std::string& service_element_name);

template <typename EventIdType, typename FieldIdType, typename ServiceIdType>
bool operator==(const BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>& lhs,
                const BindingServiceTypeDeployment<EventIdType, FieldIdType, ServiceIdType>& rhs) noexcept;

}  // namespace score::mw::com::impl

#include "score/mw/com/impl/configuration/binding_service_type_deployment_impl.h"

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_BINDING_SERVICE_TYPE_DEPLOYMENT_H
