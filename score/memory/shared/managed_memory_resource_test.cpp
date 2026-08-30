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
#include "managed_memory_resource.h"

#include "fake/my_memory_resource.h"
#include "shared_memory_test_resources.h"

#include "gtest/gtest.h"

#include <memory>

namespace score::memory::shared::test
{

TEST(ManagedMemoryResource, offersToGetMemoryResourceManager)
{
    std::unique_ptr<ManagedMemoryResource> unit = std::make_unique<MyMemoryResource>();
    ManagedMemoryResourceTestAttorney attorney(*unit);
    EXPECT_NE(attorney.getMemoryResourceProxy(), nullptr);
}

TEST(ManagedMemoryResource, CanDestructImplByParentClass)
{
    std::unique_ptr<ManagedMemoryResource> unit = std::make_unique<MyMemoryResource>();
    unit.reset();
    EXPECT_EQ(unit.get(), nullptr);
}

TEST(ManagedMemoryResource, CanConstructAndDestructSimpleType)
{
    std::unique_ptr<ManagedMemoryResource> unit = std::make_unique<MyMemoryResource>();
    auto* theAnswer = unit->construct<std::uint64_t>(42U);
    EXPECT_EQ(*theAnswer, 42U);
    unit->destruct(*theAnswer);
}

TEST(ManagedMemoryResource, CanConstructAndDestructComplexType)
{
    std::unique_ptr<ManagedMemoryResource> unit = std::make_unique<MyMemoryResource>();
    auto* theAnswer = unit->construct<std::vector<std::uint8_t>>(std::vector<std::uint8_t>{1U, 2U, 3U, 4U});
    EXPECT_EQ(theAnswer->at(0), 1U);
    EXPECT_EQ(theAnswer->at(3), 4U);
    unit->destruct(*theAnswer);
}

TEST(ManagedMemoryResource, CanConstructAndDestructComplexTypeWithMultipleConstructionParams)
{
    std::unique_ptr<ManagedMemoryResource> unit = std::make_unique<MyMemoryResource>();
    auto* theAnswer = unit->construct<std::vector<std::uint8_t>>(4U, std::allocator<std::vector<std::uint8_t>>{});
    EXPECT_EQ(theAnswer->size(), 4U);
    unit->destruct(*theAnswer);
}

}  // namespace score::memory::shared::test
