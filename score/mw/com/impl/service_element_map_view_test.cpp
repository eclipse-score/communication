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

#include "score/mw/com/impl/service_element_map_view.h"
#include "score/mw/com/impl/service_element_map_view_factory.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

TEST(ServiceElementMapViewTest, MapSizeChangesWithInsertionOfElements)
{
    RecordProperty("Verifies", "SCR-14031544");
    RecordProperty("Description",
                   "Checks that the ServiceElementMapView class behaves identically to a "
                   "std::map regarding non-mutable methods");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::map<std::string_view, std::uint8_t> initial_map;
    ServiceElementMapView map_view = ServiceElementMapViewFactory<std::uint8_t>::Create(initial_map);

    EXPECT_EQ(map_view.size(), 0);
    initial_map.insert({"0", 0});
    EXPECT_EQ(map_view.size(), 1);
    initial_map.insert({"1", 1});
    EXPECT_EQ(map_view.size(), 2);
}

TEST(ServiceElementMapViewTest, MapSizeChangesWithRemovalOfElements)
{
    RecordProperty("Verifies", "SCR-14031544");
    RecordProperty("Description", "Checks that the GenericProxy EventMap class behaves identically to a std::map");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::map<std::string_view, std::uint8_t> initial_map;
    ServiceElementMapView map_view = ServiceElementMapViewFactory<std::uint8_t>::Create(initial_map);

    initial_map.insert({"0", 0});
    initial_map.emplace("1", 1);
    initial_map.emplace("2", 2);

    EXPECT_EQ(map_view.size(), 3);
    initial_map.erase("0");
    EXPECT_EQ(map_view.size(), 2);

    initial_map.erase(map_view.cbegin());
    EXPECT_EQ(map_view.size(), 1);

    initial_map.erase(map_view.cbegin());
    EXPECT_EQ(map_view.size(), 0);
}

TEST(ServiceElementMapViewTest, MapWithElementsIsNotEmpty)
{
    RecordProperty("Verifies", "SCR-14031544");
    RecordProperty("Description", "Checks that the GenericProxy EventMap class behaves identically to a std::map");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::map<std::string_view, std::uint8_t> initial_map;
    ServiceElementMapView map_view = ServiceElementMapViewFactory<std::uint8_t>::Create(initial_map);
    EXPECT_TRUE(map_view.empty());
    initial_map.insert({"0", 0});
    EXPECT_FALSE(map_view.empty());
    initial_map.erase(initial_map.cbegin());
    EXPECT_TRUE(map_view.empty());
}

TEST(ServiceElementMapViewTest, CanFindElementsInMap)
{
    RecordProperty("Verifies", "SCR-14031544");
    RecordProperty("Description", "Checks that the GenericProxy EventMap class behaves identically to a std::map");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::map<std::string_view, std::uint8_t> initial_map;
    ServiceElementMapView map_view = ServiceElementMapViewFactory<std::uint8_t>::Create(initial_map);
    initial_map.insert({"0", 0});
    initial_map.emplace("1", 1);
    initial_map.emplace("2", 2);

    auto first_it = map_view.find("0");
    ASSERT_NE(first_it, map_view.cend());
    EXPECT_FALSE(first_it->first.compare(std::string_view{"0"}));
    EXPECT_EQ(first_it->second, 0);

    auto second_it = map_view.find("1");
    ASSERT_NE(second_it, map_view.cend());
    EXPECT_FALSE(second_it->first.compare("1"));
    EXPECT_EQ(second_it->second, 1);

    auto third_it = map_view.find("2");
    ASSERT_NE(third_it, map_view.cend());
    EXPECT_FALSE(third_it->first.compare("2"));
    EXPECT_EQ(third_it->second, 2);

    auto invalid_it = map_view.find("3");
    ASSERT_EQ(invalid_it, map_view.cend());
}

TEST(ServiceElementMapViewTest, MapCanBeCopied)
{
    std::map<std::string_view, std::uint8_t> initial_map;
    ServiceElementMapView map_view = ServiceElementMapViewFactory<std::uint8_t>::Create(initial_map);
    initial_map.insert({"0", 0});
    initial_map.emplace("1", 1);
    initial_map.emplace("2", 2);

    const ServiceElementMapView<std::uint8_t> new_map(map_view);
    EXPECT_EQ(new_map.size(), 3);
}

}  // namespace
}  // namespace score::mw::com::impl
