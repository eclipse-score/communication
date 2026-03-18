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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SERVICE_INSTANCE_ID_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SERVICE_INSTANCE_ID_H

#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"

#include "score/json/json_parser.h"
#include "score/mw/log/logging.h"

#include <score/blank.hpp>

#include <exception>
#include <string_view>
#include <variant>

namespace score::mw::com::impl
{

class ServiceInstanceId
{
  public:
    using BindingInformation = std::variant<LolaServiceInstanceId, SomeIpServiceInstanceId, score::cpp::blank>;

    explicit ServiceInstanceId(const score::json::Object& json_object) noexcept;
    explicit ServiceInstanceId(BindingInformation binding_info) noexcept;

    score::json::Object Serialize() const noexcept;
    std::string_view ToHashString() const noexcept;

    // This variable has no invariance that needs to be conserved and is needed to be both accessed and set by the user
    // thus would require a getter and setter that do not conserve any other invariants. In such a case direct access
    // is justified.
    // coverity[autosar_cpp14_a0_1_1_violation]
    // coverity[autosar_cpp14_m11_0_1_violation] same justification as the above supperssion
    BindingInformation binding_info_;

    /**
     * \brief The size of the hash string returned by ToHashString()
     *
     * The size is the max size of the hash string returned by ToHashString() from all the bindings in
     * BindingInformation plus 1 for the index of the binding type in the variant, BindingInformation.
     */
    // Variable is used in a test case -> so this line is tested and prepared for easier reuse
    // coverity[autosar_cpp14_a0_1_1_violation]
    constexpr static std::size_t hashStringSize{
        std::max(LolaServiceInstanceId::hashStringSize, SomeIpServiceInstanceId::hashStringSize) + 1U};

    constexpr static std::uint32_t serializationVersion = 1U;

  private:
    /**
     * \brief Stringified format of this ServiceInstanceId which can be used for hashing.
     */
    std::string hash_string_;
};

bool operator==(const ServiceInstanceId& lhs, const ServiceInstanceId& rhs) noexcept;
bool operator<(const ServiceInstanceId& lhs, const ServiceInstanceId& rhs) noexcept;

template <typename ServiceInstanceIdBinding>
const ServiceInstanceIdBinding& GetServiceInstanceIdBinding(const ServiceInstanceId& service_instance_id)
{
    const auto* service_instance_id_binding = std::get_if<ServiceInstanceIdBinding>(&service_instance_id.binding_info_);
    // LCOV_EXCL_BR_START False positive: The tool is reporting that the true decision is never taken. We have tests in
    // service_instance_deployment_test.cpp (GettingLolaBindingFromServiceInstanceIdNotContainingLolaBindingTerminates
    // and GettingSomeIpBindingFromServiceInstanceIdNotContainingSomeIpBindingTerminates) which test the true branch of
    // this condition. This is a bug with the tool to be fixed in (Ticket-219132). This suppression should be removed
    // when the tool is fixed.
    if (service_instance_id_binding == nullptr)
    // LCOV_EXCL_BR_STOP
    {
        ::score::mw::log::LogFatal("lola")
            << "Trying to get binding from ServiceInstanceId which contains a different binding. Terminating.";
        std::terminate();
    }
    return *service_instance_id_binding;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SERVICE_INSTANCE_ID_H
