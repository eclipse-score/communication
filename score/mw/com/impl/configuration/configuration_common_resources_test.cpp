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

TEST(LolaConfigurationCommonResourcesDeathTest,
     GetValueFromJsonReturnsValueIfKeyAndValueExist_NonArithmeticType)
{
    // Given a json with a nested object (non-arithmetic, non-string type)
    auto j1 = R"( { "config" : { "port" : 8080 } } )"_json;
    const auto& top_level_object = j1.As<score::json::Object>().value().get();

    // When GetValueFromJson is called with matching key and parseable value
    const auto& result = GetValueFromJson<score::json::Object>(top_level_object, "config");

    // Then the value is returned successfully
    EXPECT_TRUE(result.find("port") != result.end());
}

TEST(LolaConfigurationCommonResourcesDeathTest,
     GetValueFromJsonTerminatesWhenValueCannotBeParsed_NonArithmeticType)
{
    // Given a json with a key, but value that cannot be parsed to json::Object
    auto j1 = R"( { "config" : "not_an_object" } )"_json;
    const auto& top_level_object = j1.As<score::json::Object>().value().get();

    // When GetValueFromJson is called with a value that can't parse to json::Object
    // Then the function terminates with LogFatal
    EXPECT_DEATH(GetValueFromJson<score::json::Object>(top_level_object, "config"), ".*");
}

TEST(LolaConfigurationCommonResourcesDeathTest,
     GetValueFromJsonReturnsValueIfKeyAndValueExist_ArithmeticType)
{
    // Given a json with a numeric value (arithmetic type)
    auto j1 = R"( { "port" : 8080 } )"_json;
    const auto& top_level_object = j1.As<score::json::Object>().value().get();

    // When GetValueFromJson is called with matching key and parseable value
    auto result = GetValueFromJson<std::uint32_t>(top_level_object, "port");

    // Then the value is returned successfully
    EXPECT_EQ(result, 8080U);
}

TEST(LolaConfigurationCommonResourcesDeathTest,
     GetValueFromJsonTerminatesWhenValueCannotBeParsed_ArithmeticType)
{
    // Given a json with a key, but value that cannot be parsed to arithmetic type
    auto j1 = R"( { "port" : "not_a_number" } )"_json;
    const auto& top_level_object = j1.As<score::json::Object>().value().get();

    // When GetValueFromJson is called with a value that can't parse to uint32_t
    EXPECT_DEATH(GetValueFromJson<std::uint32_t>(top_level_object, "port"), ".*");
}

TEST(LolaConfigurationCommonResourcesDeathTest,
     GetValueFromJsonReturnsValueIfKeyAndValueExist_StringViewType)
{
    // Given a json with a string value
    auto j1 = R"( { "name" : "service_A" } )"_json;
    const auto& top_level_object = j1.As<score::json::Object>().value().get();

    // When GetValueFromJson is called with matching key and parseable value
    auto result = GetValueFromJson<std::string_view>(top_level_object, "name");

    // Then the value is returned successfully
    EXPECT_EQ(result, "service_A");
}

TEST(LolaConfigurationCommonResourcesDeathTest,
     GetValueFromJsonTerminatesWhenValueCannotBeParsed_StringViewType)
{
    // Given a json with a key, but value that cannot be parsed to string_view
    auto j1 = R"( { "name" : 123 } )"_json;
    const auto& top_level_object = j1.As<score::json::Object>().value().get();

    // When GetValueFromJson is called with a value that can't parse to string_view
    EXPECT_DEATH(GetValueFromJson<std::string_view>(top_level_object, "name"), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
