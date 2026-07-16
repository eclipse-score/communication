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
#include "score/mw/com/impl/configuration/config_parser.h"

#include "score/mw/com/impl/configuration/configuration_json_parsing_strategy.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"

#include "gmock/gmock.h"
#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <utility>
#include <variant>

namespace score::mw::com::impl
{

namespace
{

using score::json::operator""_json;

// The detailed parsing behaviour is verified in configuration_json_parsing_strategy_test.cpp. The tests here only
// verify that the free Parse() functions delegate to ConfigurationJsonParsingStrategy.
class ConfigParserFixture : public ::testing::Test
{
  public:
    const std::string get_path(const std::string& file_name)
    {
        const std::string default_path = "score/mw/com/impl/configuration/example/" + file_name;

        std::ifstream file(default_path);
        if (file.is_open())
        {
            file.close();
            return default_path;
        }
        else
        {
            return "external/safe_posix_platform/" + default_path;
        }
    }
};

TEST_F(ConfigParserFixture, ParseFromPathDelegatesToStrategy)
{
    RecordProperty("Description",
                   "Checks that the free Parse(path) function delegates to ConfigurationJsonParsingStrategy.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto path = get_path("mw_com_config.json");

    const auto config_from_free_function = configuration::Parse(path);
    const auto config_from_strategy = configuration::ConfigurationJsonParsingStrategy{}.Parse(path);

    EXPECT_EQ(config_from_free_function.GetServiceInstances().size(),
              config_from_strategy.GetServiceInstances().size());
    EXPECT_EQ(config_from_free_function.GetServiceTypes().size(), config_from_strategy.GetServiceTypes().size());
}

TEST_F(ConfigParserFixture, ParseFromJsonDelegatesToStrategy)
{
    RecordProperty("Description",
                   "Checks that the free Parse(json) function delegates to ConfigurationJsonParsingStrategy.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    auto json = R"({
        "serviceTypes": [],
        "serviceInstances": []
    })"_json;

    const auto config = configuration::Parse(std::move(json));

    EXPECT_TRUE(config.GetServiceInstances().empty());
    EXPECT_TRUE(config.GetServiceTypes().empty());
}

}  // namespace

}  // namespace score::mw::com::impl
