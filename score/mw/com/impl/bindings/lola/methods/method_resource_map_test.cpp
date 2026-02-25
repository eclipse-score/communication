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
#include "score/mw/com/impl/bindings/lola/methods/method_resource_map.h"

#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"

#include "score/memory/shared/shared_memory_resource.h"
#include "score/memory/shared/shared_memory_resource_mock.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>

namespace score::mw::com::impl::lola
{
namespace
{

using namespace ::testing;

constexpr GlobalConfiguration::ApplicationId kProcessIdentifier1{10};
constexpr GlobalConfiguration::ApplicationId kProcessIdentifier2{11};

constexpr ProxyInstanceIdentifier::ProxyInstanceCounter kProxyInstanceCounter1{1};
constexpr ProxyInstanceIdentifier::ProxyInstanceCounter kProxyInstanceCounter2{2};

constexpr pid_t kDummyPid1{20};
constexpr pid_t kDummyPid2{30};

class MethodResourceMapFixture : public ::testing::Test
{
  public:
    MethodResourceMapFixture& GivenAMethodResourceMap()
    {
        score::cpp::ignore = method_resource_map_.emplace();
        return *this;
    }

    MethodResourceMapFixture& WithAnInsertedRegion(const ProxyInstanceIdentifier proxy_instance_identifier,
                                                   const pid_t proxy_pid)
    {
        method_resource_map_->InsertAndCleanUpOldRegions(
            proxy_instance_identifier, proxy_pid, std::make_shared<memory::shared::SharedMemoryResourceMock>());
        return *this;
    }

    std::optional<MethodResourceMap> method_resource_map_{};
};

using MethodResourceMapInsertFixture = MethodResourceMapFixture;
TEST_F(MethodResourceMapInsertFixture, InsertReturnsIteratorToInsertedElement)
{
    GivenAMethodResourceMap();

    // When inserting a new element
    const auto method_shm_resource = std::make_shared<memory::shared::SharedMemoryResourceMock>();
    const auto [inserted_it, _] = method_resource_map_->InsertAndCleanUpOldRegions(
        ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1}, kDummyPid1, method_shm_resource);

    // Then the result should contain an iterator to the inserted element
    const auto [inserted_instance_counter, inserted_method_shm_resource] = *inserted_it;
    EXPECT_EQ(inserted_instance_counter, kProxyInstanceCounter1);
    EXPECT_EQ(inserted_method_shm_resource, method_shm_resource);
}

TEST_F(MethodResourceMapInsertFixture,
       InsertReturnsNoRegionsRemovedWhenNoElementsWithSameProcessIdentifierAndDifferentPidExist)
{
    GivenAMethodResourceMap().WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                   kDummyPid1);

    // When inserting a new element with a different proxy instance identifier and PID
    const auto method_shm_resource = std::make_shared<memory::shared::SharedMemoryResourceMock>();
    const auto [_, cleanup_result] = method_resource_map_->InsertAndCleanUpOldRegions(
        ProxyInstanceIdentifier{kProcessIdentifier2, kProxyInstanceCounter1}, kDummyPid2, method_shm_resource);

    // Then the result should contain that no regions were removed
    EXPECT_EQ(cleanup_result, MethodResourceMap::CleanUpResult::NO_REGIONS_REMOVED);
}

TEST_F(MethodResourceMapInsertFixture,
       InsertReturnsRegionsRemovedWhenElementWithSameProcessIdentifierAndDifferentPidExists)
{
    GivenAMethodResourceMap().WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                   kDummyPid1);

    // When inserting a new element with the same proxy instance identifier but different PID
    const auto [_, cleanup_result] = method_resource_map_->InsertAndCleanUpOldRegions(
        ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter2},
        kDummyPid2,
        std::make_shared<memory::shared::SharedMemoryResourceMock>());

    // Then the result should contain that regions were removed
    EXPECT_EQ(cleanup_result, MethodResourceMap::CleanUpResult::OLD_REGIONS_REMOVED);
}

TEST_F(MethodResourceMapInsertFixture, InsertRemovesElementsContainingSameProcessIdentifierAndDifferentPid)
{
    GivenAMethodResourceMap()
        .WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1}, kDummyPid1)
        .WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter2}, kDummyPid1)
        .WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier2, kProxyInstanceCounter1}, kDummyPid1)
        .WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier2, kProxyInstanceCounter2}, kDummyPid1);

    // When inserting a new element with the same proxy instance identifier but different PID
    score::cpp::ignore = method_resource_map_->InsertAndCleanUpOldRegions(
        ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
        kDummyPid2,
        std::make_shared<memory::shared::SharedMemoryResourceMock>());

    // Then only the existing elements in the map with the same ProcessIdentifier but different PID should have been
    // removed
    EXPECT_FALSE(method_resource_map_->Contains(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                kDummyPid1));
    EXPECT_FALSE(method_resource_map_->Contains(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter2},
                                                kDummyPid1));
    EXPECT_TRUE(method_resource_map_->Contains(ProxyInstanceIdentifier{kProcessIdentifier2, kProxyInstanceCounter1},
                                               kDummyPid1));
    EXPECT_TRUE(method_resource_map_->Contains(ProxyInstanceIdentifier{kProcessIdentifier2, kProxyInstanceCounter2},
                                               kDummyPid1));
}

TEST_F(MethodResourceMapInsertFixture,
       InsertingElementsContainingDifferentInstanceCounterAfterCleanupDoesNotCleanupAgain)
{
    GivenAMethodResourceMap().WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                   kDummyPid1);

    // and given that a new element was inserted with the same proxy instance identifier but different PID which lead to
    // a cleanup
    score::cpp::ignore = method_resource_map_->InsertAndCleanUpOldRegions(
        ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
        kDummyPid2,
        std::make_shared<memory::shared::SharedMemoryResourceMock>());

    // When inserting a new element with the same application ID and PID but different instance counter
    const auto [_, cleanup_result] = method_resource_map_->InsertAndCleanUpOldRegions(
        ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter2},
        kDummyPid2,
        std::make_shared<memory::shared::SharedMemoryResourceMock>());

    // Then the result should contain that no regions were removed
    EXPECT_EQ(cleanup_result, MethodResourceMap::CleanUpResult::NO_REGIONS_REMOVED);

    // and the map should contain the both regions corresponding to the inserted application ID and pid
    EXPECT_TRUE(method_resource_map_->Contains(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                               kDummyPid2));
    EXPECT_TRUE(method_resource_map_->Contains(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter2},
                                               kDummyPid2));
}

TEST_F(MethodResourceMapInsertFixture, InsertingAlreadyExistingElementTerminates)
{
    GivenAMethodResourceMap().WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                   kDummyPid1);

    // When inserting the same element that was already inserted
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = method_resource_map_->InsertAndCleanUpOldRegions(
                                     ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                     kDummyPid1,
                                     std::make_shared<memory::shared::SharedMemoryResourceMock>()));
}

using MethodResourceMapContainsFixture = MethodResourceMapFixture;
TEST_F(MethodResourceMapContainsFixture, ContainsReturnFalseWhenNoElementWasInserted)
{
    GivenAMethodResourceMap();

    // When checking if the map contains an element which has never been inserted exists
    const auto does_contain = method_resource_map_->Contains(
        ProxyInstanceIdentifier{kProcessIdentifier2, kProxyInstanceCounter1}, kDummyPid1);

    // Then the result should be false
    EXPECT_FALSE(does_contain);
}

TEST_F(MethodResourceMapContainsFixture, ContainsReturnFalseWhenElementWithDifferentProcessIdentifierIsInserted)
{
    GivenAMethodResourceMap().WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                   kDummyPid1);

    // When checking if the map contains an element with a different process identifier
    const auto does_contain = method_resource_map_->Contains(
        ProxyInstanceIdentifier{kProcessIdentifier2, kProxyInstanceCounter1}, kDummyPid1);

    // Then the result should be false
    EXPECT_FALSE(does_contain);
}

TEST_F(MethodResourceMapContainsFixture, ContainsReturnFalseWhenElementWithDifferentPidIsInserted)
{
    GivenAMethodResourceMap().WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                   kDummyPid1);

    // When checking if the map contains an element with a different process identifier
    const auto does_contain = method_resource_map_->Contains(
        ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1}, kDummyPid2);

    // Then the result should be false
    EXPECT_FALSE(does_contain);
}

TEST_F(MethodResourceMapContainsFixture, ContainsReturnsTrueWhenElementMatchingKeyIsInserted)
{
    GivenAMethodResourceMap().WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                   kDummyPid1);

    // When checking if the map contains the element that was already inserted
    const auto does_contain = method_resource_map_->Contains(
        ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1}, kDummyPid1);

    // Then the result should be true
    EXPECT_TRUE(does_contain);
}

TEST_F(MethodResourceMapContainsFixture, ContainsReturnsTrueWhenElementMatchingKeyIsOverwritten)
{
    GivenAMethodResourceMap().WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                   kDummyPid1);

    // and given that a new element with the same application ID but different PID is inserted
    const auto method_shm_resource = std::make_shared<memory::shared::SharedMemoryResourceMock>();
    score::cpp::ignore = method_resource_map_->InsertAndCleanUpOldRegions(
        ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1}, kDummyPid2, method_shm_resource);

    // When checking if the map contains the element that was inserted
    const auto does_contain = method_resource_map_->Contains(
        ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1}, kDummyPid2);

    // Then the result should be true
    EXPECT_TRUE(does_contain);
}

using MethodResourceMapClearFixture = MethodResourceMapFixture;
TEST_F(MethodResourceMapClearFixture, ClearingRemovesAllElements)
{
    GivenAMethodResourceMap()
        .WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1}, kDummyPid1)
        .WithAnInsertedRegion(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter2}, kDummyPid2);

    // When calling Clear on the map
    method_resource_map_->Clear();

    // Then the map should no longer contain the inserted regions
    EXPECT_FALSE(method_resource_map_->Contains(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter1},
                                                kDummyPid1));
    EXPECT_FALSE(method_resource_map_->Contains(ProxyInstanceIdentifier{kProcessIdentifier1, kProxyInstanceCounter2},
                                                kDummyPid2));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
