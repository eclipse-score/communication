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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SERVICE_INSTANCE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SERVICE_INSTANCE_DEPLOYMENT_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/instance_specifier.h"

#include "score/json/json_parser.h"
#include "score/mw/log/logging.h"

#include <score/blank.hpp>

#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <variant>

namespace score::mw::com::impl
{

class ServiceInstanceDeployment
{
  public:
    using BindingInformation =
        std::variant<LolaServiceInstanceDeployment, SomeIpServiceInstanceDeployment, score::cpp::blank>;

    explicit ServiceInstanceDeployment(const score::json::Object& json_object) noexcept;
    ServiceInstanceDeployment(const ServiceIdentifierType service,
                              BindingInformation binding,
                              const QualityType asil_level,
                              InstanceSpecifier instance_specifier)
        : service_{service},
          bindingInfo_{std::move(binding)},
          asilLevel_{asil_level},
          instance_specifier_{std::move(instance_specifier)}
    {
    }

    ~ServiceInstanceDeployment() noexcept = default;

    ServiceInstanceDeployment(const ServiceInstanceDeployment& other) = default;
    ServiceInstanceDeployment& operator=(const ServiceInstanceDeployment& other) = default;
    ServiceInstanceDeployment(ServiceInstanceDeployment&& other) = default;
    ServiceInstanceDeployment& operator=(ServiceInstanceDeployment&& other) = default;

    score::json::Object Serialize() const noexcept;

    BindingType GetBindingType() const noexcept;

    // Note the struct is not compliant to POD type containing non-POD member.
    // The struct is used as a config storage obtained by performing the parsing json object.
    // Public access is required by the implementation to reach the following members of the struct.
    // coverity[autosar_cpp14_m11_0_1_violation]
    ServiceIdentifierType service_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    BindingInformation bindingInfo_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    QualityType asilLevel_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    InstanceSpecifier instance_specifier_;
    constexpr static std::uint32_t serializationVersion = 1U;
};

bool operator==(const ServiceInstanceDeployment& lhs, const ServiceInstanceDeployment& rhs) noexcept;
bool operator<(const ServiceInstanceDeployment& lhs, const ServiceInstanceDeployment& rhs) noexcept;
bool areCompatible(const ServiceInstanceDeployment& lhs, const ServiceInstanceDeployment& rhs);

template <typename ServiceInstanceDeploymentBinding>
const ServiceInstanceDeploymentBinding& GetServiceInstanceDeploymentBinding(
    const ServiceInstanceDeployment& service_instance_deployment)
{
    const auto* service_instance_deployment_binding =
        std::get_if<ServiceInstanceDeploymentBinding>(&service_instance_deployment.bindingInfo_);

    // LCOV_EXCL_BR_START False positive: The tool is reporting that the true decision is never taken. We have tests in
    // service_instance_deployment_test.cpp
    // (GettingLolaBindingFromServiceInstanceDeploymentNotContainingLolaBindingTerminates,
    // GettingSomeIpBindingFromServiceInstanceDeploymentNotContainingSomeIpBindingTerminates and
    // GettingBlankBindingFromServiceInstanceDeploymentNotContainingBlankBindingTerminates) which test the true branch
    // of this condition. This is a bug with the tool to be fixed in (Ticket-219132). This suppression should be removed
    // when the tool is fixed.
    if (service_instance_deployment_binding == nullptr)
    // LCOV_EXCL_BR_STOP
    {
        ::score::mw::log::LogFatal("lola")
            << "Trying to get binding from ServiceInstanceDeployment which contains a different binding. Terminating.";
        std::terminate();
    }
    return *service_instance_deployment_binding;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SERVICE_INSTANCE_DEPLOYMENT_H
