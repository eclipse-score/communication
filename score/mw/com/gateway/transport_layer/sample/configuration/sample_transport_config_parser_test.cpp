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
 *******************************************************************************/
#include "score/mw/com/gateway/transport_layer/sample/configuration/sample_transport_config_parser.h"

#include "score/json/json_parser.h"

#include <gtest/gtest.h>

#include <fstream>

namespace score::mw::com::gateway
{
namespace
{

using score::json::operator""_json;

class SampleTransportConfigParserFixture : public ::testing::Test
{
  public:
    const std::string get_path(const std::string& file_name)
    {
        const std::string default_path =
            "score/mw/com/gateway/transport_layer/sample/configuration/example/" + file_name;

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

TEST_F(SampleTransportConfigParserFixture, ParseExampleTransportJson)
{
    const auto config = ParseSampleTransportConfig(get_path("mw_com_gateway_sample_transport_config.json"));

    EXPECT_EQ(45001U, config.local_port_);
    EXPECT_EQ(45002U, config.remote_port_);
    EXPECT_EQ(5000U, config.request_timeout_ms_);
}

TEST_F(SampleTransportConfigParserFixture, ParseHvSocketWithRequestTimeout)
{
    // Given a sample transport config JSON with a custom request-timeout-ms
    auto j = R"(
{
  "hypervisor-socket": {
    "remote-ip": "192.168.0.1",
    "local-port": 8080,
    "remote-port": 9090,
    "request-timeout-ms": 3000
  }
}
)"_json;

    // When the config is parsed
    const auto config = ParseSampleTransportConfig(std::move(j));

    // Then all socket fields must be populated correctly
    EXPECT_EQ(8080U, config.local_port_);
    EXPECT_EQ(9090U, config.remote_port_);
    EXPECT_EQ(3000U, config.request_timeout_ms_);
}

TEST_F(SampleTransportConfigParserFixture, ParseHvSocketDefaultRequestTimeout)
{
    // Given a sample transport config JSON without request-timeout-ms
    auto j = R"(
{
  "hypervisor-socket": {
    "remote-ip": "192.168.0.1",
    "local-port": 8080,
    "remote-port": 9090
  }
}
)"_json;

    // When the config is parsed
    const auto config = ParseSampleTransportConfig(std::move(j));

    // Then the default timeout of 5000ms must be applied
    EXPECT_EQ(5000U, config.request_timeout_ms_);
}

TEST_F(SampleTransportConfigParserFixture, ParseFromInvalidPathDies)
{
    EXPECT_DEATH(ParseSampleTransportConfig("/nonexistent/transport_config.json"), ".*");
}

TEST_F(SampleTransportConfigParserFixture, ParseNonObjectTopLevelDies)
{
    auto j = R"([1, 2, 3])"_json;
    EXPECT_DEATH(ParseSampleTransportConfig(std::move(j)), ".*");
}

}  // namespace
}  // namespace score::mw::com::gateway
