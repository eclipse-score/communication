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
#include "score/mw/com/impl/mocking/test_type_utilities.h"

#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_specifier.h"

#include <list>

namespace score::mw::com::impl
{
namespace
{

std::list<ServiceTypeDeployment> service_type_deployments_{};
std::list<ServiceInstanceDeployment> service_instance_deployments_{};

}  // namespace

InstanceIdentifier MakeFakeInstanceIdentifier(const std::uint16_t unique_identifier)
{
    auto service_identifier_type = make_ServiceIdentifierType("my_service_type", 0U, 0U);
    auto instance_specifier = InstanceSpecifier::Create(std::string{"dummy_specifier"}).value();

    ServiceInstanceId lola_instance_id{LolaServiceInstanceId{unique_identifier}};

    auto& type_deployment = service_type_deployments_.emplace_back(score::cpp::blank{});
    auto& instance_deployment = service_instance_deployments_.emplace_back(
        std::move(service_identifier_type),
        LolaServiceInstanceDeployment{LolaServiceInstanceId{unique_identifier}},
        QualityType::kASIL_B,
        std::move(instance_specifier));

    return make_InstanceIdentifier(instance_deployment, type_deployment);
}

HandleType MakeFakeHandle(const std::uint16_t unique_identifier)
{
    auto dummy_instance_identifier = MakeFakeInstanceIdentifier(std::move(unique_identifier));
    ServiceInstanceId lola_instance_id{LolaServiceInstanceId{unique_identifier}};
    return make_HandleType(std::move(dummy_instance_identifier), lola_instance_id);
}

void ResetInstanceIdentifierConfiguration()
{
    service_type_deployments_.clear();
    service_instance_deployments_.clear();
}

}  // namespace score::mw::com::impl
