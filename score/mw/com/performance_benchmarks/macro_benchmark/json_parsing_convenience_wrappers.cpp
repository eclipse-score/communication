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

#include <optional>
#include <string_view>
#include <utility>
namespace score::mw::com::test
{

score::json::Object parse_json_from_file(std::string_view path)
{
    score::json::JsonParser json_parser;

    auto json_res = json_parser.FromFile(path);
    if (!json_res.has_value())
    {
        score::mw::log::LogError(kJsonParserLogContext) << "Can not create a json from: " << path;
        score::mw::log::LogError(kJsonParserLogContext) << json_res.error();
        std::exit(EXIT_FAILURE);
    }

    const auto& top_level_object = json_res.value().As<score::json::Object>();

    if (!top_level_object.has_value())
    {
        score::mw::log::LogError(kJsonParserLogContext) << "Can not create a json from: " << path;
        score::mw::log::LogError(kJsonParserLogContext) << json_res.error();
        std::exit(EXIT_FAILURE);
    }
    return std::move(top_level_object.value().get());
}

std::optional<json::Object::const_iterator> find_json_key(std::string_view key,
                                                          const score::json::Object& top_level_object)
{
    auto value_it = top_level_object.find(key);
    if (value_it == top_level_object.end())
    {
        return std::nullopt;
    }
    return value_it;
}

}  // namespace score::mw::com::test
