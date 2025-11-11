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
#include "score/mw/com/performance_benchmarks/macro_benchmark/json_parsing_convenience_wrappers.h"
#include "score/json/internal/model/any.h"
#include "score/json/json_parser.h"

#include <score/stop_token.hpp>

#include <gtest/gtest.h>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace score::mw::com::test
{
namespace
{

using score::json::operator""_json;
TEST(JsonParsingConvinienceFuncionsTest, ParseJsonFromFile)
{
    std::string_view path = "score/mw/com/performance_benchmarks/macro_benchmark/config/logging.json";
    auto parsed_json = parse_json_from_file(path);
    EXPECT_TRUE(parsed_json.size() > 0);
}

using score::json::operator""_json;
TEST(JsonParsingConvinienceFuncionsTest, FindJsonKeyWithValidKeySucceeds)
{
    std::string_view key = "bla";
    // given a top level object that does contain the searched key
    const auto json_root = R"({ "bla" : 10 })"_json;
    const auto& json_object = json_root.As<json::Object>().value().get();

    // when the key is searched for
    auto found_key_optional = find_json_key(key, json_object);

    // when the key is searched for
    ASSERT_TRUE(found_key_optional.has_value());
}

TEST(JsonParsingConvinienceFuncionsTest, FindJsonKeyWithInValidKeyFails)
{
    using score::json::operator""_json;
    std::string_view key = "bla";
    // given a top level object that does not contain the searched key
    const auto json_root = R"({ "not_bla" : 10 })"_json;
    const auto& json_object = json_root.As<json::Object>().value().get();
    // when the key is searched for
    auto found_key_optional = find_json_key(key, json_object);

    // then an ampty optional is returned
    ASSERT_FALSE(found_key_optional.has_value());
}

template <typename T>
class JsonParsingConvinienceFuncionsTypedTest : public ::testing::Test
{
};

using number_types = ::testing::Types<std::uint16_t,
                                      std::uint32_t,
                                      std::uint64_t,
                                      std::int16_t,
                                      std::int32_t,
                                      std::int64_t,
                                      std::size_t,
                                      double,
                                      float,
                                      std::string,
                                      std::string_view>;

class TypeIntrospector
{
  public:
    template <typename T>
    static std::pair<std::string, T> GetJsonStr()
    {
        std::stringstream json_str;
        json_str << R"({"root_key" : )";
        if constexpr (std::is_integral_v<T>)
        {
            auto val = static_cast<T>(12);
            json_str << val << " }";
            return std::make_pair(json_str.str(), val);
        }
        if constexpr (std::is_floating_point_v<T>)
        {
            auto val = static_cast<T>(12.3);
            json_str << val << " }";
            return std::make_pair(json_str.str(), val);
        }
        if constexpr (std::is_same_v<T, std::string>)
        {
            T val{"string_bla"};
            json_str << R"(")" << val << R"(" })";
            return std::make_pair(json_str.str(), val);
        }
        if constexpr (std::is_same_v<T, std::string_view>)
        {
            T val{"string_view_bla"};
            json_str << R"(")" << val << R"(" })";
            return std::make_pair(json_str.str(), val);
        }
    }
    template <typename T>
    static std::string GetName(int i)
    {
        if constexpr (std::is_integral_v<T>)
        {
            return "IntegralType" + std::to_string(i);
        }
        if constexpr (std::is_floating_point_v<T>)
        {
            return "FloatingType" + std::to_string(i);
        }
        if constexpr (std::is_same_v<T, std::string>)
        {
            return "StringType" + std::to_string(i);
        }
        if constexpr (std::is_same_v<T, std::string_view>)
        {
            return "StringViewType" + std::to_string(i);
        }
    }
};

TYPED_TEST_SUITE(JsonParsingConvinienceFuncionsTypedTest, number_types, TypeIntrospector);

TYPED_TEST(JsonParsingConvinienceFuncionsTypedTest, ParseJsonKeySucceedsWhenValueIsOfCorrectType)
{
    auto [json_str, val] = TypeIntrospector::GetJsonStr<TypeParam>();

    const auto root_json{json::operator""_json(json_str.c_str(), json_str.size())};
    const auto& json_obj = root_json.template As<json::Object>().value().get();
    auto parsed_val = parse_json_key<TypeParam>("root_key", json_obj);

    EXPECT_EQ(parsed_val, val);
}

}  // namespace
}  // namespace score::mw::com::test
