/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "score/socom/string_registry.hpp"

namespace score::socom {

TEST(StringRegistryTest, InsertMultipleStrings) {
    String_registry registry;

    /// Verify the existence of the type cara::core::util::String_registry interface.
    auto const insert1 = registry.insert(std::string("TestString"));
    /// String from an std::String is added to the registry when it is not there yet.
    EXPECT_TRUE(insert1.second);
    /// StringView of the correct string is returned.
    EXPECT_TRUE(insert1.first.data() == std::string("TestString"));

    auto const insert2 = registry.insert(std::string("TestString"));
    /// String from an std::String is not added to the registry when it is already there.
    EXPECT_FALSE(insert2.second);
    /// StringView of the correct string is returned.
    EXPECT_TRUE(insert2.first.data() == std::string("TestString"));
}

TEST(StringRegistryTest, InsertMultipleStringViews) {
    String_registry registry;
    std::string_view const string_view("TestString");
    std::string_view const second_string_view("TestString");

    auto const insert1 = registry.insert(string_view);
    /// String from a std::string_view is added to the registry when it is not there yet.
    EXPECT_TRUE(insert1.second);
    /// std::string_view of the correct string is returned.
    EXPECT_TRUE(insert1.first.string_view() == string_view);

    auto const insert2 = registry.insert(string_view);
    /// String from a std::string_view is not added to registry when it is already there.
    EXPECT_FALSE(insert2.second);
    /// std::string_view of the correct string is returned.
    EXPECT_TRUE(insert2.first.string_view() == string_view);

    auto const insert3 = registry.insert(second_string_view);
    /// String from a different std::string_view is not added to the registry when an identical
    /// string is already there.
    EXPECT_FALSE(insert3.second);
    /// std::string_view of the correct string is returned.
    EXPECT_TRUE(insert3.first.string_view() == string_view);
}

TEST(StringRegistryTest, InsertMultipleStringViewLiterals) {
    String_registry registry;
    std::string_view const string_view("TestStringLiteral");
    std::string_view const second_string_view("TestStringLiteral");

    auto const insert1 = registry.insert(string_view, Literal_tag{});
    /// String from a std::string_view literal is added to the registry when it is not there yet.
    EXPECT_TRUE(insert1.second);
    /// std::string_view of the correct string is returned.
    EXPECT_TRUE(insert1.first.string_view() == string_view);

    auto const insert2 = registry.insert(string_view, Literal_tag{});
    /// String from a std::string_view literal is not added to the registry when it is already
    /// there.
    EXPECT_FALSE(insert2.second);
    /// std::string_view of the correct string is returned.
    EXPECT_TRUE(insert2.first.string_view() == string_view);

    auto const insert3 = registry.insert(second_string_view, Literal_tag{});
    /// String from a different instance of std::string_view literal is not added to the registry
    /// when an identical string is already there.
    EXPECT_FALSE(insert3.second);
    /// std::string_view of the correct string is returned.
    EXPECT_TRUE(insert3.first.string_view() == string_view);
}

TEST(StringRegistryTest, ServiceIDRegistry) {
    std::string_view const string_view("TestString");
    // Get a reference to the service ID registry singleton
    String_registry& service_id_registry = score::socom::service_id_registry();
    auto const insert_service_id_registry = service_id_registry.insert(string_view);
    EXPECT_TRUE(insert_service_id_registry.second);
    EXPECT_TRUE(insert_service_id_registry.first.string_view() == string_view);

    // Get another reference to the same service ID registry singleton
    String_registry& service_id_registry2 = score::socom::service_id_registry();
    // Attempt to insert the same string view into the registry again
    auto const insert_service_id_registry2 = service_id_registry2.insert(string_view);
    // `second` should be `false` indicating that the string was already present
    EXPECT_FALSE(insert_service_id_registry2.second);
    EXPECT_TRUE(insert_service_id_registry2.first.string_view() == string_view);
}

TEST(StringRegistryTest, InstanceIDRegistry) {
    std::string_view const string_view("TestString");
    // Get a reference to the instance ID registry singleton
    String_registry& instance_id_registry = score::socom::instance_id_registry();
    auto const insert_instance_id_registry = instance_id_registry.insert(string_view);
    EXPECT_TRUE(insert_instance_id_registry.second);
    EXPECT_TRUE(insert_instance_id_registry.first.string_view() == string_view);

    // Get another reference to the same instance ID registry singleton
    String_registry& instance_id_registry2 = score::socom::instance_id_registry();
    // Attempt to insert the same string view into the registry again
    auto const insert_instance_id_registry2 = instance_id_registry2.insert(string_view);
    // `second` should be `false` indicating that the string was already present
    EXPECT_FALSE(insert_instance_id_registry2.second);
    EXPECT_TRUE(insert_instance_id_registry2.first.string_view() == string_view);
}

}  // namespace score::socom
