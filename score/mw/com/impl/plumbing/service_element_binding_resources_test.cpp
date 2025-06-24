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

TEST(ServiceElementBindingResourcesGetElementFqIdFromConfigTest,
     ConvertingEventLolaConfigToElementFqIdReturnsValidElementFqId)
{
    // When converting Event config containing a lola binding to an ElementFqId
    const auto actual_element_fq_id = GetElementFqIdFromLolaConfig<lola::ElementType::EVENT>(
        kConfigurationStoreLolaBinding.lola_service_type_deployment_,
        *kConfigurationStoreLolaBinding.lola_instance_id_,
        kDummyEventName);

    // Then we get a valid ElementFqId containing data from the configuration
    const lola::ElementFqId expected_element_fq_id{
        kServiceId, kDummyEventId, kLolaServiceInstanceId.GetId(), lola::ElementType::EVENT};
    EXPECT_EQ(expected_element_fq_id, actual_element_fq_id);
}

TEST(ServiceElementBindingResourcesGetElementFqIdFromConfigTest,
     ConvertingFieldLolaConfigToElementFqIdReturnsValidElementFqId)
{
    // When converting Field config containing a lola binding to an ElementFqId
    const auto actual_element_fq_id = GetElementFqIdFromLolaConfig<lola::ElementType::FIELD>(
        kConfigurationStoreLolaBinding.lola_service_type_deployment_,
        *kConfigurationStoreLolaBinding.lola_instance_id_,
        kDummyFieldName);

    // Then we get a valid ElementFqId containing data from the configuration
    const lola::ElementFqId expected_element_fq_id{
        kServiceId, kDummyFieldId, kLolaServiceInstanceId.GetId(), lola::ElementType::FIELD};
    EXPECT_EQ(expected_element_fq_id, actual_element_fq_id);
}

TEST(ServiceElementBindingResourcesGetElementFqIdFromConfigDeathTest, ConvertingConfigWithInvalidElementTypeTerminates)
{
    // When converting Event config containing a lola binding to an ElementFqId using GetElementFqIdFromLolaConfig typed
    // with an invalid ElementType Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = GetElementFqIdFromLolaConfig<lola::ElementType::INVALID>(
                     kConfigurationStoreLolaBinding.lola_service_type_deployment_,
                     *kConfigurationStoreLolaBinding.lola_instance_id_,
                     kDummyEventName),
                 ".*");
}

TEST(ServiceElementBindingResourcesGetElementFqIdFromConfigDeathTest, ConvertingConfigWithUnknownElementTypeTerminates)
{
    // When converting Event config containing a lola binding to an ElementFqId using GetElementFqIdFromLolaConfig typed
    // with an unknown ElementType Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = GetElementFqIdFromLolaConfig<static_cast<lola::ElementType>(100U)>(
                     kConfigurationStoreLolaBinding.lola_service_type_deployment_,
                     *kConfigurationStoreLolaBinding.lola_instance_id_,
                     kDummyEventName),
                 ".*");
}

}  // namespace score::mw::com::impl
