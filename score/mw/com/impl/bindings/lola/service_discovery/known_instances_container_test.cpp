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
#include "score/mw/com/impl/bindings/lola/service_discovery/known_instances_container.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{

using ::testing::Contains;
using ::testing::Not;

InstanceSpecifier kInstanceSpecifier1{InstanceSpecifier::Create("/bla/blub/specifier").value()};
InstanceSpecifier kInstanceSpecifier2{InstanceSpecifier::Create("/bla/blub/specifier2").value()};
LolaServiceTypeDeployment kServiceId1{1U};
LolaServiceTypeDeployment kServiceId2{2U};
ServiceTypeDeployment kServiceTypeDeployment1{kServiceId1};
ServiceTypeDeployment kServiceTypeDeployment2{kServiceId2};

LolaServiceInstanceId kInstanceId1{1U};
LolaServiceInstanceId kInstanceId2{2U};
LolaServiceInstanceId kInstanceId3{3U};

ServiceInstanceDeployment kInstanceDeployment1{make_ServiceIdentifierType("/bla/blub/service1"),
                                               LolaServiceInstanceDeployment{kInstanceId1},
                                               QualityType::kASIL_QM,
                                               kInstanceSpecifier1};
ServiceInstanceDeployment kInstanceDeployment2{make_ServiceIdentifierType("/bla/blub/service1"),
                                               LolaServiceInstanceDeployment{kInstanceId2},
                                               QualityType::kASIL_QM,
                                               kInstanceSpecifier1};
ServiceInstanceDeployment kInstanceDeployment3{make_ServiceIdentifierType("/bla/blub/service3"),
                                               LolaServiceInstanceDeployment{kInstanceId1},
                                               QualityType::kASIL_QM,
                                               kInstanceSpecifier2};
ServiceInstanceDeployment kInstanceDeploymentAny{make_ServiceIdentifierType("/bla/blub/service1"),
                                                 LolaServiceInstanceDeployment{},
                                                 QualityType::kASIL_QM,
                                                 kInstanceSpecifier1};
InstanceIdentifier kInstanceIdentifier1{make_InstanceIdentifier(kInstanceDeployment1, kServiceTypeDeployment1)};
InstanceIdentifier kInstanceIdentifier2{make_InstanceIdentifier(kInstanceDeployment2, kServiceTypeDeployment1)};
InstanceIdentifier kInstanceIdentifier3{make_InstanceIdentifier(kInstanceDeployment3, kServiceTypeDeployment2)};
InstanceIdentifier kInstanceIdentifierAny{make_InstanceIdentifier(kInstanceDeploymentAny, kServiceTypeDeployment1)};
EnrichedInstanceIdentifier kEnrichedInstanceIdentifier1{kInstanceIdentifier1};
EnrichedInstanceIdentifier kEnrichedInstanceIdentifier2{kInstanceIdentifier2};
EnrichedInstanceIdentifier kEnrichedInstanceIdentifier3{kInstanceIdentifier3};
EnrichedInstanceIdentifier kEnrichedInstanceIdentifierAny{kInstanceIdentifierAny};
HandleType kHandleType1{make_HandleType(kInstanceIdentifier1)};
HandleType kHandleType2{make_HandleType(kInstanceIdentifier2)};
HandleType kHandleTypeAny1{make_HandleType(kInstanceIdentifierAny, kInstanceId1)};
HandleType kHandleTypeAny2{make_HandleType(kInstanceIdentifierAny, kInstanceId2)};
HandleType kHandleTypeAny3{make_HandleType(kInstanceIdentifierAny, kInstanceId3)};

class KnownInstancesContainerTest : public ::testing::Test
{
  public:
    KnownInstancesContainer unit_{};
};

TEST_F(KnownInstancesContainerTest, ContainerIsEmptyByDefault)
{
    EXPECT_TRUE(unit_.Empty());
}

TEST_F(KnownInstancesContainerTest, ContainerIsNotEmptyIfOneInstanceIsAdded)
{
    unit_.Insert(kEnrichedInstanceIdentifier1);
    EXPECT_FALSE(unit_.Empty());
}

TEST_F(KnownInstancesContainerTest, ContainerIsNotEmptyIfMultipleInstancesAreAdded)
{
    unit_.Insert(kEnrichedInstanceIdentifier1);
    unit_.Insert(kEnrichedInstanceIdentifier2);
    EXPECT_FALSE(unit_.Empty());
}

TEST_F(KnownInstancesContainerTest, CanInsertInstance)
{
    unit_.Insert(kEnrichedInstanceIdentifier1);
    EXPECT_THAT(unit_.GetKnownHandles(kEnrichedInstanceIdentifierAny), Contains(kHandleTypeAny1));
}

TEST_F(KnownInstancesContainerTest, CanRemoveInstance)
{
    unit_.Insert(kEnrichedInstanceIdentifier1);
    unit_.Remove(kEnrichedInstanceIdentifier1);
    EXPECT_FALSE(unit_.Empty());
}

TEST_F(KnownInstancesContainerTest, GetKnownHandlesReturnsMatchingSpecificInstance)
{
    unit_.Insert(kEnrichedInstanceIdentifier1);
    unit_.Insert(kEnrichedInstanceIdentifier2);
    EXPECT_THAT(unit_.GetKnownHandles(kEnrichedInstanceIdentifier1), Contains(kHandleType1));
    EXPECT_THAT(unit_.GetKnownHandles(kEnrichedInstanceIdentifier1), Not(Contains(kHandleType2)));
}

TEST_F(KnownInstancesContainerTest, GetKnownHandlesReturnsMatchingAnyInstance)
{
    unit_.Insert(kEnrichedInstanceIdentifier1);
    unit_.Insert(kEnrichedInstanceIdentifier2);
    EXPECT_THAT(unit_.GetKnownHandles(kEnrichedInstanceIdentifierAny), Contains(kHandleTypeAny1));
    EXPECT_THAT(unit_.GetKnownHandles(kEnrichedInstanceIdentifierAny), Contains(kHandleTypeAny2));
}

TEST_F(KnownInstancesContainerTest, GetKnownHandlesDoesNotReturnNonMatchingInstances)
{
    unit_.Insert(kEnrichedInstanceIdentifier3);
    EXPECT_THAT(unit_.GetKnownHandles(kEnrichedInstanceIdentifierAny), Not(Contains(kHandleTypeAny3)));
}

TEST_F(KnownInstancesContainerTest, CanMergeTwoContainers)
{
    unit_.Insert(kEnrichedInstanceIdentifier1);

    KnownInstancesContainer unit{};
    unit.Insert(kEnrichedInstanceIdentifier2);

    unit_.Merge(std::move(unit));

    EXPECT_THAT(unit_.GetKnownHandles(kEnrichedInstanceIdentifierAny), Contains(kHandleTypeAny1));
    EXPECT_THAT(unit_.GetKnownHandles(kEnrichedInstanceIdentifierAny), Contains(kHandleTypeAny2));
}

TEST_F(KnownInstancesContainerTest, InsertingIdentifierWithoutInstanceIdReturnsFalse)
{
    // When inserting an EnrichedInstanceIdentifier which doesn't contain a service instance id
    const auto insertion_result = unit_.Insert(kEnrichedInstanceIdentifierAny);

    // Then the returned result will be false
    EXPECT_FALSE(insertion_result);
}

TEST_F(KnownInstancesContainerTest, InsertingIdentifierWithoutInstanceIdDoesNotInsertIdentifierInMap)
{
    // When inserting an EnrichedInstanceIdentifier which doesn't contain a service instance id
    score::cpp::ignore = unit_.Insert(kEnrichedInstanceIdentifierAny);

    // Then the known instance container will still be empty
    EXPECT_TRUE(unit_.Empty());
}

}  // namespace score::mw::com::impl::lola
