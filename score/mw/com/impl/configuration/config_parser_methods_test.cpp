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

#include "score/mw/com/impl/configuration/service_identifier_type.h"

#include "gmock/gmock.h"
#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <variant>

#include <fstream>

namespace score::mw::com::impl
{

namespace
{

using score::json::operator""_json;

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

    ServiceIdentifierType si_{make_ServiceIdentifierType("/bmw/ncar/services/TirePressureService", 12U, 34U)};
    ServiceVersionType sv_{make_ServiceVersionType(12U, 34U)};
    std::pair<const ServiceIdentifierType*, const ServiceVersionType*> found_service_type_{&si_, &sv_};
};

using ConfigParserFixtureDeathTest = ConfigParserFixture;

TEST_F(ConfigParserFixtureDeathTest, NoMethodNameWillCauseTermination)
{
    // Given a JSON with a missing method name
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
             "binding": "SHM",
             "serviceId": 1234,
             "events": [],
             "fields": [],
             "methods": [
                { "methodId": 40 }
             ]
        }
      ]
    }
  ],
  "serviceInstances": []
}
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST_F(ConfigParserFixtureDeathTest, ValidMethodNameWillNotCauseTermination)
{
    // Given a JSON with a valid method name
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
             "binding": "SHM",
             "serviceId": 1234,
             "events": [],
             "fields": [],
             "methods": [
                {
                  "methodName": "SetPressure",
                  "methodId": 40
                }
             ]
        }
      ]
    }
  ],
  "serviceInstances": []
}
)"_json;

    // When parsing the JSON
    // Then parsing succeeds without termination
    EXPECT_NO_THROW(score::mw::com::impl::configuration::Parse(std::move(j2)));
}

TEST_F(ConfigParserFixtureDeathTest, NoMethodIdWillCauseTermination)
{
    // Given a JSON with a missing method id
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
          "binding": "SHM",
          "serviceId": 1234,
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure"
            }
          ]
        }
      ]
    }
  ],
  "serviceInstances": []
}
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST_F(ConfigParserFixtureDeathTest, ValidMethodIdWillNotCauseTermination)
{
    // Given a JSON with a valid method id
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
          "binding": "SHM",
          "serviceId": 1234,
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure",
              "methodId": 40
            }
          ]
        }
      ]
    }
  ],
  "serviceInstances": []
}
)"_json;

    // When parsing the JSON
    // Then parsing succeeds without termination
    EXPECT_NO_THROW(score::mw::com::impl::configuration::Parse(std::move(j2)));
}

TEST_F(ConfigParserFixtureDeathTest, DuplicateMethodTypeDeploymentWillCauseTermination)
{
    // Given a JSON with a duplicate LoLa method type deployment (duplicate methodName)
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
             "binding": "SHM",
             "serviceId": 1234,
             "events": [],
             "fields": [],
             "methods": [
                {
                  "methodName": "SetPressure",
                  "methodId": 40
                },
                {
                  "methodName": "SetPressure",
                  "methodId": 41
                }
             ]
        }
      ]
    }
  ],
  "serviceInstances": []
}
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST_F(ConfigParserFixtureDeathTest, NoDuplicateMethodTypeDeploymentWillNotCauseTermination)
{
    // Given a JSON with no duplicate LoLa method type deployment (unique methodNames)
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
             "binding": "SHM",
             "serviceId": 1234,
             "events": [],
             "fields": [],
             "methods": [
                {
                  "methodName": "SetPressure",
                  "methodId": 40
                },
                {
                  "methodName": "GetPressure",
                  "methodId": 41
                }
             ]
        }
      ]
    }
  ],
  "serviceInstances": []
}
)"_json;

    // When parsing the JSON
    // Then parsing succeeds without termination
    EXPECT_NO_THROW(score::mw::com::impl::configuration::Parse(std::move(j2)));
}

TEST_F(ConfigParserFixtureDeathTest, DuplicateMethodIdWillCauseTermination)
{
    // Given a JSON with duplicate method IDs
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
             "binding": "SHM",
             "serviceId": 1234,
             "events": [],
             "fields": [],
             "methods": [
                {
                  "methodName": "SetPressure",
                  "methodId": 40
                },
                {
                  "methodName": "GetPressure",
                  "methodId": 40
                }
             ]
        }
      ]
    }
  ],
  "serviceInstances": []
}
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST_F(ConfigParserFixtureDeathTest, NoDuplicateMethodIdWillNotCauseTermination)
{
    // Given a JSON with no duplicate method IDs
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
             "binding": "SHM",
             "serviceId": 1234,
             "events": [],
             "fields": [],
             "methods": [
                {
                  "methodName": "SetPressure",
                  "methodId": 40
                },
                {
                  "methodName": "GetPressure",
                  "methodId": 41
                }
             ]
        }
      ]
    }
  ],
  "serviceInstances": []
}
)"_json;

    // When parsing the JSON
    // Then parsing succeeds without termination
    EXPECT_NO_THROW(score::mw::com::impl::configuration::Parse(std::move(j2)));
}

TEST_F(ConfigParserFixtureDeathTest, DuplicateServiceInstanceMethodsWillDie)
{
    // Given a JSON with duplicate method instance deployments
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
          "binding": "SHM",
          "serviceId": 1234,
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure",
              "methodId": 40
            }
          ]
        }
      ]
    }
  ],
  "serviceInstances": [
    {
      "instanceSpecifier": "abc/abc/TirePressurePort",
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "instances": [
        {
          "instanceId": 1234,
          "asil-level": "QM",
          "binding": "SHM",
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure",
              "queueSize": 1
            },
            {
              "methodName": "SetPressure",
              "queueSize": 2
            }
          ]
        }
      ]
    }
  ]
}
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST_F(ConfigParserFixtureDeathTest, NoDuplicateServiceInstanceMethodsWillNotDie)
{
    // Given a JSON without duplicate method instance deployments
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
          "binding": "SHM",
          "serviceId": 1234,
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure",
              "methodId": 40
            },
            {
              "methodName": "GetPressure",
              "methodId": 41
            }
          ]
        }
      ]
    }
  ],
  "serviceInstances": [
    {
      "instanceSpecifier": "abc/abc/TirePressurePort",
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "instances": [
        {
          "instanceId": 1234,
          "asil-level": "QM",
          "binding": "SHM",
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure",
              "queueSize": 1
            },
            {
              "methodName": "GetPressure",
              "queueSize": 2
            }
          ]
        }
      ]
    }
  ]
}
)"_json;

    // When parsing the JSON
    // Then the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST_F(ConfigParserFixture, MethodQueueSizeIsNulloptWhenNotProvided)
{
    // Given a JSON with a method without explicit queueSize
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
          "binding": "SHM",
          "serviceId": 1234,
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure",
              "methodId": 40
            }
          ]
        }
      ]
    }
  ],
  "serviceInstances": [
    {
      "instanceSpecifier": "abc/abc/TirePressurePort",
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "instances": [
        {
          "instanceId": 1234,
          "asil-level": "QM",
          "binding": "SHM",
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure"
            }
          ]
        }
      ]
    }
  ]
}
)"_json;

    // When parsing the JSON
    const auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    // Then the queue size should be nullopt (not provided)
    const auto deployments =
        config.GetServiceInstances().at(InstanceSpecifier::Create("abc/abc/TirePressurePort").value());
    const auto& lola_deployment = std::get<LolaServiceInstanceDeployment>(deployments.bindingInfo_);
    EXPECT_FALSE(lola_deployment.methods_.at("SetPressure").queue_size_.has_value());
}

TEST_F(ConfigParserFixture, MethodQueueSizeCanBeSpecified)
{
    // Given a JSON with a method with explicit queueSize
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
          "binding": "SHM",
          "serviceId": 1234,
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure",
              "methodId": 40
            }
          ]
        }
      ]
    }
  ],
  "serviceInstances": [
    {
      "instanceSpecifier": "abc/abc/TirePressurePort",
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "instances": [
        {
          "instanceId": 1234,
          "asil-level": "QM",
          "binding": "SHM",
          "events": [],
          "fields": [],
          "methods": [
            {
              "methodName": "SetPressure",
              "queueSize": 5
            }
          ]
        }
      ]
    }
  ]
}
)"_json;

    // When parsing the JSON
    const auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    // Then the queue size should be set to the specified value
    const auto deployments =
        config.GetServiceInstances().at(InstanceSpecifier::Create("abc/abc/TirePressurePort").value());
    const auto& lola_deployment = std::get<LolaServiceInstanceDeployment>(deployments.bindingInfo_);
    EXPECT_EQ(lola_deployment.methods_.at("SetPressure").queue_size_, 5);
}

}  // namespace
}  // namespace score::mw::com::impl
