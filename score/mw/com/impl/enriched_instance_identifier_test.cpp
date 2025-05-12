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
#include "score/mw/com/impl/enriched_instance_identifier.h"

#include <gtest/gtest.h>

#include <variant>

namespace score::mw::com::impl
{

const auto kInstanceSpecifier = InstanceSpecifier::Create("/bla/blub/specifier").value();
const auto kServiceTypeName = "/bla/blub/service";
const auto kService = make_ServiceIdentifierType(kServiceTypeName);
const auto kLolaServiceId{15U};
const ServiceTypeDeployment kLolaServiceTypeDeployment{LolaServiceTypeDeployment{kLolaServiceId}};
const ServiceTypeDeployment kBlankServiceTypeDeployment{score::cpp::blank{}};

const auto kLolaInstanceId{16U};
const auto kLolaServiceInstanceId = LolaServiceInstanceId{kLolaInstanceId};
const ServiceInstanceDeployment kLolaServiceInstanceDeployment{kService,
                                                               LolaServiceInstanceDeployment{kLolaServiceInstanceId},
                                                               QualityType::kASIL_QM,
                                                               kInstanceSpecifier};
const ServiceInstanceDeployment kBlankServiceInstanceDeployment{kService,
                                                                score::cpp::blank{},
                                                                QualityType::kASIL_QM,
                                                                kInstanceSpecifier};
const ServiceInstanceDeployment kLolaServiceInstanceDeploymentNoInstanceId{kService,
                                                                           LolaServiceInstanceDeployment{},
                                                                           QualityType::kASIL_QM,
                                                                           kInstanceSpecifier};
const auto kLolaInstanceIdentifier =
    make_InstanceIdentifier(kLolaServiceInstanceDeployment, kLolaServiceTypeDeployment);
const auto kBlankInstanceIdentifier =
    make_InstanceIdentifier(kBlankServiceInstanceDeployment, kBlankServiceTypeDeployment);
const auto kInstanceIdentifierFindAny =
    make_InstanceIdentifier(kLolaServiceInstanceDeploymentNoInstanceId, kLolaServiceTypeDeployment);

TEST(EnrichedInstanceIdentifierConstructFromInstanceIdentifierTest, WillUseInstanceIdFromConfig)
{
    EnrichedInstanceIdentifier unit{kLolaInstanceIdentifier};

    EXPECT_EQ(unit.GetInstanceIdentifier(), kLolaInstanceIdentifier);

    const auto service_instance_id_result = unit.GetInstanceId();
    ASSERT_TRUE(service_instance_id_result.has_value());

    const auto retrieved_lola_service_instance_id =
        std::get<LolaServiceInstanceId>(service_instance_id_result.value().binding_info_);
    EXPECT_EQ(retrieved_lola_service_instance_id, kLolaServiceInstanceId);
}

TEST(EnrichedInstanceIdentifierConstructFromInstanceIdentifierTest, WillUseEmptyOptionalInstanceIdFromConfig)
{
    EnrichedInstanceIdentifier unit{kInstanceIdentifierFindAny};

    EXPECT_EQ(unit.GetInstanceIdentifier(), kInstanceIdentifierFindAny);

    const auto service_instance_id_result = unit.GetInstanceId();
    ASSERT_FALSE(service_instance_id_result.has_value());
}

TEST(EnrichedInstanceIdentifierConstructFromPairTest, WillUseInstanceIdFromConstructorParameter)
{
    const LolaServiceInstanceId other_lola_service_instance_id{20U};
    EnrichedInstanceIdentifier unit{kInstanceIdentifierFindAny, ServiceInstanceId{other_lola_service_instance_id}};

    EXPECT_EQ(unit.GetInstanceIdentifier(), kInstanceIdentifierFindAny);

    const auto service_instance_id_result = unit.GetInstanceId();
    ASSERT_TRUE(service_instance_id_result.has_value());

    const auto retrieved_lola_service_instance_id =
        std::get<LolaServiceInstanceId>(service_instance_id_result.value().binding_info_);
    EXPECT_EQ(retrieved_lola_service_instance_id, other_lola_service_instance_id);
}

TEST(EnrichedInstanceIdentifierConstructFromPairDeathTest, WillTerminateIfInstanceIdIsProvidedAndAlsoExistsInConfig)
{
    const LolaServiceInstanceId other_lola_service_instance_id{20U};
    EXPECT_DEATH(
        EnrichedInstanceIdentifier unit(kLolaInstanceIdentifier, ServiceInstanceId{other_lola_service_instance_id}),
        ".*");
}

TEST(EnrichedInstanceIdentifierConstructFromHandleTest, WillUseInstanceIdFromHandle)
{
    const LolaServiceInstanceId other_lola_service_instance_id{20U};
    const auto handle = make_HandleType(kInstanceIdentifierFindAny, ServiceInstanceId{other_lola_service_instance_id});

    EnrichedInstanceIdentifier unit{handle};

    EXPECT_EQ(unit.GetInstanceIdentifier(), kInstanceIdentifierFindAny);

    const auto service_instance_id_result = unit.GetInstanceId();
    ASSERT_TRUE(service_instance_id_result.has_value());

    const auto retrieved_lola_service_instance_id =
        std::get<LolaServiceInstanceId>(service_instance_id_result.value().binding_info_);
    EXPECT_EQ(retrieved_lola_service_instance_id, other_lola_service_instance_id);
}

TEST(EnrichedInstanceIdentifierGetQualityTest, GetsQualityFromInstanceIdentifier)
{
    const EnrichedInstanceIdentifier unit{kLolaInstanceIdentifier};
    EXPECT_EQ(unit.GetQualityType(), QualityType::kASIL_QM);
}

TEST(EnrichedInstanceIdentifierGetQualityTest, CanOverwriteQualityFromInstanceIdentifier)
{
    const EnrichedInstanceIdentifier unit{EnrichedInstanceIdentifier{kLolaInstanceIdentifier}, QualityType::kASIL_B};
    EXPECT_EQ(unit.GetQualityType(), QualityType::kASIL_B);
}

TEST(EnrichedInstanceIdentifierBindingSpecificServiceIdTest, CanRetrieveServiceIdFromCorrectBinding)
{
    const EnrichedInstanceIdentifier unit{kLolaInstanceIdentifier};
    ASSERT_TRUE(unit.GetBindingSpecificServiceId<LolaServiceTypeDeployment>().has_value());
    EXPECT_EQ(unit.GetBindingSpecificServiceId<LolaServiceTypeDeployment>().value(), kLolaServiceId);
}

TEST(EnrichedInstanceIdentifierBindingSpecificServiceIdTest, CannotRetrieveServiceIdWithWrongBinding)
{
    const EnrichedInstanceIdentifier unit{kBlankInstanceIdentifier};
    EXPECT_FALSE(unit.GetBindingSpecificServiceId<LolaServiceTypeDeployment>().has_value());
}

TEST(EnrichedInstanceIdentifierBindingSpecificInstanceIdTest, CanRetrieveInstanceIdFromCorrectBinding)
{
    const EnrichedInstanceIdentifier unit{kLolaInstanceIdentifier};
    ASSERT_TRUE(unit.GetBindingSpecificInstanceId<LolaServiceInstanceId>().has_value());
    EXPECT_EQ(unit.GetBindingSpecificInstanceId<LolaServiceInstanceId>().value(), kLolaInstanceId);
}

TEST(EnrichedInstanceIdentifierBindingSpecificInstanceIdTest, CannotRetrieveInstanceIdWithWrongBinding)
{
    const EnrichedInstanceIdentifier unit{kBlankInstanceIdentifier};
    EXPECT_FALSE(unit.GetBindingSpecificInstanceId<LolaServiceInstanceId>().has_value());
}

TEST(EnrichedInstanceIdentifierBindingSpecificInstanceIdTest, CannotRetrieveInstanceIdIfNotPresent)
{
    const EnrichedInstanceIdentifier unit{kInstanceIdentifierFindAny};
    EXPECT_FALSE(unit.GetBindingSpecificInstanceId<LolaServiceInstanceId>().has_value());
}

TEST(EnrichedInstanceIdentifierBindingSpecificInstanceIdTest, CannotRetrieveInstanceIdIfGettingWrongBindingInstanceId)
{
    // Given an EnrichedInstanceIdentifier containing a Lola binding
    const EnrichedInstanceIdentifier unit{kLolaInstanceIdentifier};

    // When trying to get a SomeIpServiceInstanceId
    const auto service_instance_id_result = unit.GetBindingSpecificInstanceId<SomeIpServiceInstanceId>();

    // Then an empty optional is returned
    EXPECT_FALSE(service_instance_id_result.has_value());
}

TEST(EnrichedInstanceIdentifierComparison, ComparesEqual)
{
    const EnrichedInstanceIdentifier lhs{kLolaInstanceIdentifier};
    const EnrichedInstanceIdentifier rhs{kLolaInstanceIdentifier};
    EXPECT_TRUE(lhs == rhs);
}

TEST(EnrichedInstanceIdentifierComparison, ComparesInequalIfInstanceIdentifierDifferent)
{
    const EnrichedInstanceIdentifier lhs{kLolaInstanceIdentifier};
    const EnrichedInstanceIdentifier rhs{kInstanceIdentifierFindAny};
    EXPECT_FALSE(lhs == rhs);
}

TEST(EnrichedInstanceIdentifierComparison, ComparesInequalIfInstanceIdDifferent)
{
    const LolaServiceInstanceId lhs_lola_service_instance_id{20U};
    const EnrichedInstanceIdentifier lhs{kInstanceIdentifierFindAny, ServiceInstanceId{lhs_lola_service_instance_id}};

    const LolaServiceInstanceId rhs_lola_service_instance_id{21U};
    const EnrichedInstanceIdentifier rhs{kInstanceIdentifierFindAny, ServiceInstanceId{rhs_lola_service_instance_id}};
    EXPECT_FALSE(lhs == rhs);
}

TEST(EnrichedInstanceIdentifierComparison, ComparesInequalIfQualityTypeDifferent)
{
    const EnrichedInstanceIdentifier lhs{EnrichedInstanceIdentifier{kLolaInstanceIdentifier}, QualityType::kASIL_B};
    const EnrichedInstanceIdentifier rhs{EnrichedInstanceIdentifier{kLolaInstanceIdentifier}, QualityType::kASIL_QM};
    EXPECT_FALSE(lhs == rhs);
}

}  // namespace score::mw::com::impl
