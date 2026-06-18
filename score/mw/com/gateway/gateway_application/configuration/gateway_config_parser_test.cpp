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
#include "score/mw/com/gateway/gateway_application/configuration/gateway_config_parser.h"

#include "gmock/gmock.h"
#include <gtest/gtest.h>

#include <fstream>

namespace score::mw::com::gateway
{

namespace
{

using score::json::operator""_json;

class GatewayConfigParserFixture : public ::testing::Test
{
  public:
    const std::string get_path(const std::string& file_name)
    {
        const std::string default_path = "score/mw/com/gateway/gateway_application/configuration/example/" + file_name;

        std::ifstream file(default_path);
        if (file.is_open())
        {
            file.close();
            return default_path;
        }
        else
        {
            return "external/score_communication/" + default_path;
        }
    }
};

TEST_F(GatewayConfigParserFixture, ParseExampleJson)
{
    const auto config = ParseGatewayConfig(get_path("mw_com_gateway_config.json"));

    const auto forwarded_services = config.GetForwardedServices();
    EXPECT_EQ(2, forwarded_services.size());

    const auto received_services = config.GetReceivedServices();
    EXPECT_EQ(1, received_services.size());

    EXPECT_EQ("sample_hypervisor", config.GetTransportLayerId());
    EXPECT_EQ("/etc/mw_com/gateway/mw_com_gateway_transport_config.json", config.GetTransportConfigPath());
}

TEST_F(GatewayConfigParserFixture, ParseFromInvalidPathDies)
{
    EXPECT_DEATH(ParseGatewayConfig("/nonexistent/gateway_config.json"), ".*");
}

TEST_F(GatewayConfigParserFixture, ParseNonObjectTopLevelDies)
{
    auto j = R"([1, 2, 3])"_json;
    EXPECT_DEATH(ParseGatewayConfig(std::move(j)), ".*");
}

}  // namespace
}  // namespace score::mw::com::gateway
