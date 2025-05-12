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
#ifndef SCORE_MW_COM_IMPL_TEST_DUMMY_INSTANCE_IDENTIFIER_BUILDER
#define SCORE_MW_COM_IMPL_TEST_DUMMY_INSTANCE_IDENTIFIER_BUILDER

#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"

#include <memory>

namespace score::mw::com::impl
{

class DummyInstanceIdentifierBuilder
{
  public:
    DummyInstanceIdentifierBuilder();

    InstanceIdentifier CreateValidLolaInstanceIdentifier();
    InstanceIdentifier CreateValidLolaInstanceIdentifierWithEvent();
    InstanceIdentifier CreateValidLolaInstanceIdentifierWithEvent(
        const LolaServiceInstanceDeployment::EventInstanceMapping& events);
    InstanceIdentifier CreateValidLolaInstanceIdentifierWithField();
    InstanceIdentifier CreateValidLolaInstanceIdentifierWithField(
        const LolaServiceInstanceDeployment::EventInstanceMapping& events);
    InstanceIdentifier CreateLolaInstanceIdentifierWithoutInstanceId();
    InstanceIdentifier CreateLolaInstanceIdentifierWithoutTypeDeployment();
    InstanceIdentifier CreateBlankBindingInstanceIdentifier();
    InstanceIdentifier CreateSomeIpBindingInstanceIdentifier();

  private:
    LolaServiceInstanceDeployment service_instance_deployment_;
    SomeIpServiceInstanceDeployment some_ip_service_instance_deployment_information_;

    LolaServiceTypeDeployment service_type_deployment_;
    ServiceTypeDeployment type_deployment_;
    ServiceIdentifierType type_;
    InstanceSpecifier instance_specifier_;
    std::unique_ptr<ServiceInstanceDeployment> instance_deployment_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_TEST_DUMMY_INSTANCE_IDENTIFIER_BUILDER
