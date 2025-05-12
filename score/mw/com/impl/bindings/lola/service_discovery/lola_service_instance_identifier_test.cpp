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
#include "score/mw/com/impl/bindings/lola/service_discovery/lola_service_instance_identifier.h"
#include "score/mw/com/impl/configuration/binding_service_type_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

const std::string kServiceTypeName = "/bla/blub/service";
const ServiceIdentifierType kService = make_ServiceIdentifierType(kServiceTypeName);
const LolaServiceInstanceId::InstanceId kLolaInstanceId{16U};
const LolaServiceInstanceId kLolaServiceInstanceId{kLolaInstanceId};
const InstanceSpecifier kInstanceSpecifier = InstanceSpecifier::Create("/bla/blub/specifier").value();
const std::uint16_t kLolaServiceId{15U};
const ServiceTypeDeployment kLolaServiceTypeDeployment{LolaServiceTypeDeployment{kLolaServiceId}};
const ServiceInstanceDeployment kLolaServiceInstanceDeployment{kService,
                                                               LolaServiceInstanceDeployment{kLolaServiceInstanceId},
                                                               QualityType::kASIL_QM,
                                                               kInstanceSpecifier};
const InstanceIdentifier kLolaInstanceIdentifier =
    make_InstanceIdentifier(kLolaServiceInstanceDeployment, kLolaServiceTypeDeployment);

TEST(LolaServiceInstanceIdentifierTest, ConstructWitServiceId)
{
    // Given some ids
    LolaServiceId service_id{13U};

    // When creating an identifier
    LolaServiceInstanceIdentifier identifier{service_id};

    // Then it contains the ids
    EXPECT_EQ(identifier.GetServiceId(), service_id);
    ASSERT_FALSE(identifier.GetInstanceId().has_value());
}

TEST(LolaServiceInstanceIdentifierTest, ConstructWithEnrichedInstanceIdentifier)
{
    // Create enriched instance identifier
    EnrichedInstanceIdentifier enriched_instance_identifier{kLolaInstanceIdentifier};

    // When creating an identifier
    LolaServiceInstanceIdentifier identifier{enriched_instance_identifier};

    // Then it contains the id
    EXPECT_EQ(identifier.GetServiceId(), kLolaServiceId);
    EXPECT_TRUE(identifier.GetInstanceId().has_value());
    EXPECT_EQ(identifier.GetInstanceId().value(), kLolaInstanceId);
}

TEST(LolaServiceInstanceIdentifierTest, ComparesEqual)
{
    // Given two identical identifiers
    LolaServiceId service_id{13U};
    LolaServiceInstanceIdentifier identifier_1{service_id};
    LolaServiceInstanceIdentifier identifier_2{service_id};

    // Then they compare equal
    EXPECT_TRUE(identifier_1 == identifier_2);
}

TEST(LolaServiceInstanceIdentifierTest, ComparesInequableWithDifferentServiceIds)
{
    // Given two identifiers with different service_id
    LolaServiceId service_id_1{13U};
    LolaServiceInstanceIdentifier identifier_1{service_id_1};
    LolaServiceId service_id_2{15U};
    LolaServiceInstanceIdentifier identifier_2{service_id_2};

    // Then they compare inequable
    EXPECT_FALSE(identifier_1 == identifier_2);
}

TEST(LolaServiceInstanceIdentifierTest, ComparesInequableWithDifferentInstanceIds)
{
    // Create the two identifiers
    EnrichedInstanceIdentifier enriched_instance_identifier_1{kLolaInstanceIdentifier};
    LolaServiceInstanceIdentifier identifier_1{enriched_instance_identifier_1};

    const LolaServiceInstanceId::InstanceId kLolaInstanceId_2{17U};
    const LolaServiceInstanceId kLolaServiceInstanceId_2{kLolaInstanceId_2};
    const ServiceInstanceDeployment kLolaServiceInstanceDeployment_2{
        kService, LolaServiceInstanceDeployment{kLolaServiceInstanceId_2}, QualityType::kASIL_QM, kInstanceSpecifier};
    const InstanceIdentifier kLolaInstanceIdentifier_2 =
        make_InstanceIdentifier(kLolaServiceInstanceDeployment_2, kLolaServiceTypeDeployment);
    EnrichedInstanceIdentifier enriched_instance_identifier_2{kLolaInstanceIdentifier_2};
    LolaServiceInstanceIdentifier identifier_2{enriched_instance_identifier_2};

    // Then they compare inequable
    EXPECT_FALSE(identifier_1 == identifier_2);
}

TEST(LolaServiceInstanceIdentifierTest, HashComparesEqual)
{
    // Given two identical identifiers
    LolaServiceId service_id{13};
    LolaServiceInstanceIdentifier identifier_1{service_id};
    LolaServiceInstanceIdentifier identifier_2{service_id};

    // Then their hashes compare equal
    EXPECT_TRUE(std::hash<LolaServiceInstanceIdentifier>{}(identifier_1) ==
                std::hash<LolaServiceInstanceIdentifier>{}(identifier_2));
}

TEST(LolaServiceInstanceIdentifierTest, HashComparesInequableWithInstanceIdZeroAndNoInstanceId)
{
    // Given two identifiers where one lacks an instance id and the other has id zero
    LolaServiceId service_id{13U};
    LolaServiceInstanceIdentifier identifier_1{service_id};

    const LolaServiceInstanceId::InstanceId kLolaInstanceId_2{0U};
    const LolaServiceInstanceId kLolaServiceInstanceId_2{kLolaInstanceId_2};
    const ServiceInstanceDeployment kLolaServiceInstanceDeployment_2{
        kService, LolaServiceInstanceDeployment{kLolaServiceInstanceId_2}, QualityType::kASIL_QM, kInstanceSpecifier};
    const InstanceIdentifier kLolaInstanceIdentifier_2 =
        make_InstanceIdentifier(kLolaServiceInstanceDeployment_2, kLolaServiceTypeDeployment);
    EnrichedInstanceIdentifier enriched_instance_identifier_2{kLolaInstanceIdentifier_2};
    LolaServiceInstanceIdentifier identifier_2{enriched_instance_identifier_2};

    // Then their hashes compare inequable
    EXPECT_FALSE(std::hash<LolaServiceInstanceIdentifier>{}(identifier_1) ==
                 std::hash<LolaServiceInstanceIdentifier>{}(identifier_2));
}

TEST(LolaServiceInstanceIdentifierTest, HashComparesInequableWithDifferentServiceIds)
{
    // Given two identifiers with different service_id
    LolaServiceId service_id_1{13U};
    LolaServiceInstanceIdentifier identifier_1{service_id_1};
    LolaServiceId service_id_2{15U};
    LolaServiceInstanceIdentifier identifier_2{service_id_2};

    // Then their hashes compare inequable
    EXPECT_FALSE(std::hash<LolaServiceInstanceIdentifier>{}(identifier_1) ==
                 std::hash<LolaServiceInstanceIdentifier>{}(identifier_2));
}

TEST(LolaServiceInstanceIdentifierTest, HashComparesInequableWithDifferentInstanceIds)
{
    // Given two identifiers with different instance_id
    EnrichedInstanceIdentifier enriched_instance_identifier_1{kLolaInstanceIdentifier};
    LolaServiceInstanceIdentifier identifier_1{enriched_instance_identifier_1};

    const LolaServiceInstanceId::InstanceId kLolaInstanceId_2{17U};
    const LolaServiceInstanceId kLolaServiceInstanceId_2{kLolaInstanceId_2};
    const ServiceInstanceDeployment kLolaServiceInstanceDeployment_2{
        kService, LolaServiceInstanceDeployment{kLolaServiceInstanceId_2}, QualityType::kASIL_QM, kInstanceSpecifier};
    const InstanceIdentifier kLolaInstanceIdentifier_2 =
        make_InstanceIdentifier(kLolaServiceInstanceDeployment_2, kLolaServiceTypeDeployment);
    EnrichedInstanceIdentifier enriched_instance_identifier_2{kLolaInstanceIdentifier_2};
    LolaServiceInstanceIdentifier identifier_2{enriched_instance_identifier_2};

    // Then they compare inequable
    EXPECT_FALSE(std::hash<LolaServiceInstanceIdentifier>{}(identifier_1) ==
                 std::hash<LolaServiceInstanceIdentifier>{}(identifier_2));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
