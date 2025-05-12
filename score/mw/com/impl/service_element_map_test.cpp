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
///
/// @file
/// @copyright Copyright (C) 2023, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
///
#include "score/mw/com/impl/service_element_map.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(ServiceElementMapTest, MapSizeChangesWithInsertionOfElements)
{
    RecordProperty("Verifies", "SCR-14031544");
    RecordProperty("Description", "Checks that the GenericProxy EventMap class behaves identically to a std::map");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    ServiceElementMap<std::uint8_t> map;

    EXPECT_EQ(map.size(), 0);
    map.insert({"0", 0});
    EXPECT_EQ(map.size(), 1);
    map.emplace("1", 1);
    EXPECT_EQ(map.size(), 2);
}

TEST(ServiceElementMapTest, MapSizeChangesWithRemovalOfElements)
{
    RecordProperty("Verifies", "SCR-14031544");
    RecordProperty("Description", "Checks that the GenericProxy EventMap class behaves identically to a std::map");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    ServiceElementMap<std::uint8_t> map;
    map.insert({"0", 0});
    map.emplace("1", 1);
    map.emplace("2", 2);

    EXPECT_EQ(map.size(), 3);
    map.erase("0");
    EXPECT_EQ(map.size(), 2);

    map.erase(map.cbegin());
    EXPECT_EQ(map.size(), 1);

    map.erase(map.cbegin());
    EXPECT_EQ(map.size(), 0);
}

TEST(ServiceElementMapTest, MapWithElementsIsNotEmpty)
{
    RecordProperty("Verifies", "SCR-14031544");
    RecordProperty("Description", "Checks that the GenericProxy EventMap class behaves identically to a std::map");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    ServiceElementMap<std::uint8_t> map;
    EXPECT_TRUE(map.empty());
    map.insert({"0", 0});
    EXPECT_FALSE(map.empty());
    map.erase(map.cbegin());
    EXPECT_TRUE(map.empty());
}

TEST(ServiceElementMapTest, CanFindElementsInMap)
{
    RecordProperty("Verifies", "SCR-14031544");
    RecordProperty("Description", "Checks that the GenericProxy EventMap class behaves identically to a std::map");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    ServiceElementMap<std::uint8_t> map;
    map.insert({"0", 0});
    map.emplace("1", 1);
    map.emplace("2", 2);

    auto first_it = map.find("0");
    ASSERT_NE(first_it, map.cend());
    EXPECT_FALSE(first_it->first.compare(std::string_view{"0"}));
    EXPECT_EQ(first_it->second, 0);

    auto second_it = map.find("1");
    ASSERT_NE(second_it, map.cend());
    EXPECT_FALSE(second_it->first.compare("1"));
    EXPECT_EQ(second_it->second, 1);

    auto third_it = map.find("2");
    ASSERT_NE(third_it, map.cend());
    EXPECT_FALSE(third_it->first.compare("2"));
    EXPECT_EQ(third_it->second, 2);

    auto invalid_it = map.find("3");
    ASSERT_EQ(invalid_it, map.cend());
}

TEST(ServiceElementMapTest, MapCanBeCopied)
{
    ServiceElementMap<std::uint8_t> map;
    map.insert({"0", 0});
    map.insert({"1", 1});

    const ServiceElementMap<std::uint8_t> new_map(map);
    EXPECT_EQ(new_map.size(), 2);
}

}  // namespace
}  // namespace score::mw::com::impl
