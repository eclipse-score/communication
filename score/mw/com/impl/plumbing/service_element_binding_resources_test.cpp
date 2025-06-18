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
#include "score/mw/com/impl/plumbing/service_element_binding_resources.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include <score/blank.hpp>

#include <gtest/gtest.h>
#include <string_view>

namespace score::mw::com::impl
{

namespace
{

const std::string kDummyEventName{"Event1"};
const std::string kDummyFieldName{"Field1"};

constexpr LolaEventId kDummyEventId{4U};
constexpr LolaFieldId kDummyFieldId{5U};

const LolaServiceId kServiceId{1U};
const LolaServiceInstanceId kLolaServiceInstanceId{1U};
const auto kInstanceSpecifier = InstanceSpecifier::Create("/bla/blub/specifier").value();
const ServiceIdentifierType kServiceIdentifier = make_ServiceIdentifierType("foo");
const QualityType kDummyQualityType = QualityType::kASIL_QM;

const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    kLolaServiceInstanceId,
    {{kDummyEventName, LolaEventInstanceDeployment{{1U}, {3U}, 1U, true, 0}}},
    {{kDummyFieldName, LolaFieldInstanceDeployment{{1U}, {3U}, 1U, true, 0}}}};
const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId,
                                                           {{kDummyEventName, kDummyEventId}},
                                                           {{kDummyFieldName, kDummyFieldId}}};

ConfigurationStore kConfigurationStoreLolaBinding{kInstanceSpecifier,
                                                  kServiceIdentifier,
                                                  kDummyQualityType,
                                                  kLolaServiceTypeDeployment,
                                                  kLolaServiceInstanceDeployment};

}  // namespace

TEST(ServiceElementBindingResourcesGetServiceElementIdTest, GettingEventIdWithValidEventNameReturnsValidEventId)
{
    // When getting the EventId from a LolaServiceTypeDeployment containing that event
    const auto actual_event_id = GetServiceElementId<ServiceElementType::EVENT>(
        kConfigurationStoreLolaBinding.lola_service_type_deployment_, kDummyEventName);

    // Then we get a valid EventId from the configuration
    EXPECT_EQ(actual_event_id, kDummyEventId);
}

TEST(ServiceElementBindingResourcesGetServiceElementIdTest, GettingFieldIdWithValidFieldNameReturnsValidFieldId)
{
    // When getting the FieldId from a LolaServiceTypeDeployment containing that field
    const auto actual_field_id = GetServiceElementId<ServiceElementType::FIELD>(
        kConfigurationStoreLolaBinding.lola_service_type_deployment_, kDummyFieldName);

    // Then we get a valid FieldId from the configuration
    EXPECT_EQ(actual_field_id, kDummyFieldId);
}

TEST(ServiceElementBindingResourcesGetElementFqIdFromConfigDeathTest, ConvertingConfigWithInvalidElementTypeTerminates)
{
    // When getting the EventId from a LolaServiceTypeDeployment which doesn't contain that event
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = GetServiceElementId<ServiceElementType::EVENT>(
                     kConfigurationStoreLolaBinding.lola_service_type_deployment_, kDummyFieldName),
                 ".*");
}

TEST(ServiceElementBindingResourcesGetElementFqIdFromConfigDeathTest, ConvertingConfigWithUnknownElementTypeTerminates)
{
    // When getting the FieldId from a LolaServiceTypeDeployment which doesn't contain that field
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = GetServiceElementId<ServiceElementType::FIELD>(
                     kConfigurationStoreLolaBinding.lola_service_type_deployment_, kDummyEventName),
                 ".*");
}

}  // namespace score::mw::com::impl
