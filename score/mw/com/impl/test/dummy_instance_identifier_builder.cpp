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
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include "score/mw/com/impl/configuration/lola_service_instance_id.h"

#include <score/blank.hpp>

#include <memory>

namespace score::mw::com::impl
{

DummyInstanceIdentifierBuilder::DummyInstanceIdentifierBuilder()
    : service_instance_deployment_{},
      some_ip_service_instance_deployment_information_{},
      service_type_deployment_{0x0},
      type_deployment_{score::cpp::blank{}},
      type_{make_ServiceIdentifierType("foo")},
      instance_specifier_{InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value()},
      instance_deployment_{}
{
}

InstanceIdentifier DummyInstanceIdentifierBuilder::CreateValidLolaInstanceIdentifier()
{
    service_instance_deployment_.instance_id_ = LolaServiceInstanceId{0x42};
    service_instance_deployment_.allowed_consumer_ = {{QualityType::kASIL_QM, {42}}};
    type_deployment_.binding_info_ = service_type_deployment_;
    instance_deployment_ = std::make_unique<ServiceInstanceDeployment>(
        type_, service_instance_deployment_, QualityType::kASIL_QM, instance_specifier_);
    return make_InstanceIdentifier(*instance_deployment_, type_deployment_);
}

InstanceIdentifier DummyInstanceIdentifierBuilder::CreateValidLolaInstanceIdentifierWithEvent()
{
    return CreateValidLolaInstanceIdentifierWithEvent({{"test_event", LolaEventInstanceDeployment{1, 1, 1, true, 0}}});
}

InstanceIdentifier DummyInstanceIdentifierBuilder::CreateValidLolaInstanceIdentifierWithField()
{
    return CreateValidLolaInstanceIdentifierWithField({{"test_field", LolaFieldInstanceDeployment{1, 1, 1, true, 0}}});
}

InstanceIdentifier DummyInstanceIdentifierBuilder::CreateValidLolaInstanceIdentifierWithEvent(
    const LolaServiceInstanceDeployment::EventInstanceMapping& events)
{
    service_instance_deployment_.instance_id_ = LolaServiceInstanceId{0x42};
    service_instance_deployment_.allowed_consumer_ = {{QualityType::kASIL_QM, {42}}};
    service_instance_deployment_.events_ = events;

    // The GenericSkeleton needs the event names to be present in the Type Deployment
    // to perform the stable string lookup. We sync it here.
    service_type_deployment_.events_.clear();
    for (const auto& event_pair : events) {
        // Add the event name to the type deployment map.
        // We assume default construction of the value (EventId) is sufficient for this mock.
        service_type_deployment_.events_[event_pair.first] = {}; 
    }

    type_deployment_.binding_info_ = service_type_deployment_;
    instance_deployment_ = std::make_unique<ServiceInstanceDeployment>(
        type_, service_instance_deployment_, QualityType::kASIL_QM, instance_specifier_);
    return make_InstanceIdentifier(*instance_deployment_, type_deployment_);
}

InstanceIdentifier DummyInstanceIdentifierBuilder::CreateValidLolaInstanceIdentifierWithField(
    const LolaServiceInstanceDeployment::FieldInstanceMapping& fields)
{
    service_instance_deployment_.instance_id_ = LolaServiceInstanceId{0x42};
    service_instance_deployment_.allowed_consumer_ = {{QualityType::kASIL_QM, {42}}};
    service_instance_deployment_.fields_ = fields;
    type_deployment_.binding_info_ = service_type_deployment_;
    instance_deployment_ = std::make_unique<ServiceInstanceDeployment>(
        type_, service_instance_deployment_, QualityType::kASIL_QM, instance_specifier_);
    return make_InstanceIdentifier(*instance_deployment_, type_deployment_);
}

InstanceIdentifier DummyInstanceIdentifierBuilder::CreateLolaInstanceIdentifierWithoutInstanceId()
{
    type_deployment_.binding_info_ = service_type_deployment_;
    instance_deployment_ = std::make_unique<ServiceInstanceDeployment>(
        type_, service_instance_deployment_, QualityType::kASIL_QM, instance_specifier_);
    return make_InstanceIdentifier(*instance_deployment_, type_deployment_);
}

InstanceIdentifier DummyInstanceIdentifierBuilder::CreateLolaInstanceIdentifierWithoutTypeDeployment()
{
    instance_deployment_ = std::make_unique<ServiceInstanceDeployment>(
        type_, service_instance_deployment_, QualityType::kASIL_QM, instance_specifier_);
    return make_InstanceIdentifier(*instance_deployment_, type_deployment_);
}

InstanceIdentifier DummyInstanceIdentifierBuilder::CreateBlankBindingInstanceIdentifier()
{
    instance_deployment_ =
        std::make_unique<ServiceInstanceDeployment>(type_, score::cpp::blank{}, QualityType::kASIL_QM, instance_specifier_);
    return make_InstanceIdentifier(*instance_deployment_, type_deployment_);
}

InstanceIdentifier DummyInstanceIdentifierBuilder::CreateSomeIpBindingInstanceIdentifier()
{
    instance_deployment_ = std::make_unique<ServiceInstanceDeployment>(
        type_, some_ip_service_instance_deployment_information_, QualityType::kASIL_QM, instance_specifier_);
    return make_InstanceIdentifier(*instance_deployment_, type_deployment_);
}

}  // namespace score::mw::com::impl
