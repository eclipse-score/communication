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
#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/json/json_parser.h"

#include <score/optional.hpp>

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{
using score::json::operator""_json;

TEST(LolaConfigurationCommonResources, GetOptionalValueFromJsonGetsAValueIfItExists)
{
    // Given a json with an existing key value pair

    const auto j1 = R"( { "bla" : 7 } )"_json;
    const auto& top_level_object = j1.As<score::json::Object>().value().get();

    const std::string_view key = "bla";
    // When GetOptionalValueFromJson is called with the json object and an appropriate key
    auto result = GetOptionalValueFromJson<std::uint8_t>(top_level_object, key);

    // Then the value corresponding to the value will be returned, wrapped in amp optional
    EXPECT_EQ(result, std::optional<std::uint8_t>(7));
}

TEST(LolaConfigurationCommonResources, GetOptionalValueFromJsonReturnsEmptyOptionalIfTheKeyIsNotFound)
{
    // Given a json
    auto j1 = R"( { "bla" : 7 } )"_json;
    const auto& top_level_object = j1.As<score::json::Object>().value().get();

    // When GetOptionalValueFromJson is called with the json object and an appropriate key
    auto result = GetOptionalValueFromJson<std::uint8_t>(top_level_object, "blabla");

    // Then an empty optional will be returned
    EXPECT_EQ(result, std::optional<std::uint8_t>{});
}

TEST(LolaConfigurationCommonResourcesDeathTest, GetOptionalValueFromJsonReturnsEmptyOptionalInCaseOfParsingFailure)
{
    // Given a json with an existing key value pair
    auto j1 = R"( { "bla" : "text" } )"_json;
    const auto& top_level_object = j1.As<score::json::Object>().value().get();

    // When GetOptionalValueFromJson is called with the json object and an appropriate key, but a value that can not
    // be interpreted as the type provided to GetOptionalValueFromJson as a template
    // Then the function will terminate
    EXPECT_DEATH(GetOptionalValueFromJson<std::uint8_t>(top_level_object, "bla"), ".*");
}

TEST(LolaConfigurationCommonResourcesDeathTest,
     DeserializingVariantTerminatesWhenProvidedVariantIndexIsEqualToSizeOfVariant)
{
    auto test_function = [] {
        // Given a json with an existing key value pair
        auto j1 = R"( { "bla" : "text" } )"_json;
        const score::json::Object& top_level_object = j1.As<score::json::Object>().value().get();

        // When trying to deserialize a variant by passing a variant index which is equal to the size of the variant
        using DummyVariant = std::variant<int, float, score::cpp::blank>;
        score::cpp::ignore = DeserializeVariant<3, DummyVariant>(top_level_object, 0U, kBindingInfoKey);  //,
    };
    // Then the program terminates
    EXPECT_DEATH(test_function(), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
