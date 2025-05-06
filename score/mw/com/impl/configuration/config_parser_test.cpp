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
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <fstream>
#include <iostream>

namespace score::mw::com::impl
{

namespace
{

using score::json::operator""_json;
using std::string_view_literals::operator""sv;

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

const std::string kTracingTraceFilterConfigPathDefaultValue{"./etc/mw_com_trace_filter.json"};

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

TEST_F(ConfigParserFixture, ParseExampleJson)
{
    RecordProperty("Verifies", "SCR-21803701, SCR-21803702, SCR-5898925, SCR-5899090, SCR-5899184, SCR-7088394");
    RecordProperty("Description", "Checks whether all necessary configuration can be read at runtime");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto config = score::mw::com::impl::configuration::Parse(get_path("ara_com_config.json"));

    const auto deployments =
        config.GetServiceInstances().at(InstanceSpecifier::Create("abc/abc/TirePressurePort").value());

    EXPECT_EQ(deployments.service_, si_);
    EXPECT_EQ(ServiceIdentifierTypeView{deployments.service_}.GetVersion(), make_ServiceVersionType(12U, 34U));

    const auto secondDeploymentInfo = std::get<LolaServiceInstanceDeployment>(deployments.bindingInfo_);
    EXPECT_EQ(deployments.asilLevel_, QualityType::kASIL_B);
    EXPECT_EQ(secondDeploymentInfo.instance_id_.value(), LolaServiceInstanceId{1234U});
    EXPECT_EQ(*secondDeploymentInfo.shared_memory_size_, 10000);

    ASSERT_EQ(secondDeploymentInfo.allowed_consumer_.size(), 2);
    ASSERT_EQ(secondDeploymentInfo.allowed_consumer_.at(QualityType::kASIL_QM).size(), 2);
    ASSERT_EQ(secondDeploymentInfo.allowed_consumer_.at(QualityType::kASIL_B).size(), 2);
    EXPECT_EQ(secondDeploymentInfo.allowed_consumer_.at(QualityType::kASIL_QM)[0], 42);
    EXPECT_EQ(secondDeploymentInfo.allowed_consumer_.at(QualityType::kASIL_QM)[1], 43);
    EXPECT_EQ(secondDeploymentInfo.allowed_consumer_.at(QualityType::kASIL_B)[0], 54);
    EXPECT_EQ(secondDeploymentInfo.allowed_consumer_.at(QualityType::kASIL_B)[1], 55);

    ASSERT_EQ(secondDeploymentInfo.allowed_provider_.size(), 2);
    ASSERT_EQ(secondDeploymentInfo.allowed_provider_.at(QualityType::kASIL_QM).size(), 1);
    ASSERT_EQ(secondDeploymentInfo.allowed_provider_.at(QualityType::kASIL_B).size(), 1);
    EXPECT_EQ(secondDeploymentInfo.allowed_provider_.at(QualityType::kASIL_QM)[0], 15);
    EXPECT_EQ(secondDeploymentInfo.allowed_provider_.at(QualityType::kASIL_B)[0], 15);

    EXPECT_EQ(secondDeploymentInfo.events_.at("CurrentPressureFrontLeft").GetNumberOfTracingSlots(), 0);
    EXPECT_EQ(secondDeploymentInfo.events_.at("CurrentPressureFrontLeft").GetNumberOfSampleSlots().value(), 50);
    EXPECT_EQ(secondDeploymentInfo.events_.at("CurrentPressureFrontLeft").max_subscribers_.value(), 5);
    EXPECT_EQ(secondDeploymentInfo.events_.at("CurrentPressureFrontLeft").enforce_max_samples_.value(), true);
    EXPECT_EQ(secondDeploymentInfo.events_.at("CurrentPressureFrontLeft").max_concurrent_allocations_.value(), 1);

    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft").GetNumberOfTracingSlots(), 7);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft").GetNumberOfSampleSlots().value(), 60 + 7);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft").max_subscribers_.value(), 6);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft").enforce_max_samples_.value(), true);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft").max_concurrent_allocations_.value(), 1);

    const auto service_deployment = config.GetServiceTypes().at(deployments.service_);
    const auto* const lola_service_type_deployment =
        std::get_if<LolaServiceTypeDeployment>(&service_deployment.binding_info_);
    ASSERT_NE(lola_service_type_deployment, nullptr);
    EXPECT_EQ(lola_service_type_deployment->service_id_, 1234);
    EXPECT_EQ(lola_service_type_deployment->events_.at("CurrentPressureFrontLeft"), 20);
    EXPECT_EQ(lola_service_type_deployment->fields_.at("CurrentTemperatureFrontLeft"), 30);

    EXPECT_EQ(config.GetGlobalConfiguration().GetProcessAsilLevel(), QualityType::kASIL_B);
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_QM), 8);
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_B), 5);
    EXPECT_EQ(config.GetGlobalConfiguration().GetSenderMessageQueueSize(), 12);

    EXPECT_EQ(config.GetGlobalConfiguration().GetShmSizeCalcMode(), ShmSizeCalculationMode::kSimulation);
}

TEST(ConfigParserDeathTest, InvalidPathWillDie)
{
    // Given an invalid path that doesn't point to a JSON file
    std::string invalid_path{"my_invalid_path_to_nowhere"};

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(invalid_path)), ".*");
}

TEST(ConfigParserDeathTest, NoServiceInstanceWillDie)
{
    // Given a JSON without necessary attribute `serviceInstances`
    auto j2 = R"(
  {
  }
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, NoServiceNameInInstanceWillDie)
{
    // Given a JSON without necessary attribute `serviceName`
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
        }
    ],
    "serviceInstances": [
        {}
    ]
  }
)"_json;
    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, NoServiceTypesWillDie)
{
    // Given a JSON without necessary attribute `serviceTypes`
    auto j2 = R"(
  {
    "serviceInstances": []
  }
)"_json;
    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, NoServiceNameForServiceType)
{
    // Given a JSON without necessary attribute `serviceTypeName`
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
             "binding": "SHM",
             "serviceId": 1234
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

TEST_F(ConfigParserFixtureDeathTest, NoVersionForServiceTypeDeployment)
{
    // Given a JSON without necessary attribute `version`
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "bindings": [
        {
             "binding": "SHM",
             "serviceId": 1234
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

TEST_F(ConfigParserFixtureDeathTest, NoBindingsForServiceTypeDeployment)
{
    // Given a JSON without necessary attribute `bindings`
    auto j2 = R"(
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      }
    }
  ],
  "serviceInstances": []
}
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST_F(ConfigParserFixtureDeathTest, NoBindingIdentifierInServiceTypeDeployment)
{
    // Given a JSON without necessary attribute `binding`
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
             "serviceId": 1234
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

TEST_F(ConfigParserFixtureDeathTest, NoServiceIdInServiceTypeDeployment)
{
    // Given a JSON without necessary attribute `serviceId`
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
             "binding": "SHM"
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

TEST_F(ConfigParserFixtureDeathTest, UnknownBindingIdentifierInServiceTypeDeployment)
{
    // Given a JSON with an unknown binding identifier
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
             "binding": "unkown",
             "serviceId": 1234
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

TEST_F(ConfigParserFixtureDeathTest, NoEventsOrFieldsWillCauseTermination)
{
    // Given a JSON without any events or fields
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
             "serviceId": 1234
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

TEST_F(ConfigParserFixtureDeathTest, NoEventNameWillCauseTermination)
{
    // Given a JSON with a missing event name
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
             "events": [
                { "eventId": 20 }
             ],
             "fields": []
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

TEST_F(ConfigParserFixtureDeathTest, NoFieldNameWillCauseTermination)
{
    // Given a JSON with a missing field name
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
             "fields": [
                { "fieldId": 20 }
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

TEST_F(ConfigParserFixtureDeathTest, NoEventIdWillCauseTermination)
{
    // Given a JSON with a missing event id
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
          "events": [
            {
              "eventName": "foo"
            }
          ],
          "fields": []
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

TEST_F(ConfigParserFixtureDeathTest, NoFieldIdWillCauseTermination)
{
    // Given a JSON with a missing field id
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
             "fields": [
                { "fieldName": "foo" }
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

TEST_F(ConfigParserFixtureDeathTest, WrongPermissionValueWillCauseTermination)
{
    // Given a JSON with an invalid permission in permission-check attribute
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
          "events": [
            {
              "eventName": "CurrentPressureFrontLeft",
              "eventId": 20
            }
          ],
          "fields": [
            {
              "fieldName": "CurrentPressureFrontRight",
              "fieldId": 21
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
          "events": [
            {
              "eventName": "CurrentPressureFrontLeft",
              "maxSubscribers": 5,
              "numberOfIpcTracingSlots": 0
            }
          ],
          "fields": [
            {
              "fieldName": "CurrentPressureFrontRight",
              "numberOfSampleSlots": 2,
              "maxSubscribers": 3,
              "numberOfIpcTracingSlots": 1
            }
          ],
          "permission-checks": "wrong_permission"
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

TEST_F(ConfigParserFixtureDeathTest, DuplicateEventTypeDeploymentWillCauseTermination)
{
    // Given a JSON with an duplicate LoLa event type deployment (duplicate eventName)
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
             "events": [
                {
                  "eventName": "foo",
                  "eventId": 20
                },
                {
                  "eventName": "foo",
                  "eventId": 21
                }
             ],
             "fields": []
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

TEST_F(ConfigParserFixtureDeathTest, DuplicateFieldTypeDeploymentWillCauseTermination)
{
    // Given a JSON with an duplicate LoLa field type deployment (duplicate fieldName)
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
             "fields": [
              {
                  "fieldName": "foo",
                  "fieldId": 20
                },
                {
                  "fieldName": "foo",
                  "fieldId": 21
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

TEST_F(ConfigParserFixtureDeathTest, DuplicateServiceTypeDeploymentWillCauseTermination)
{
    // Given a JSON with a duplicate service type deployment (duplicate serviceTypeName/version)
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
             "fields": []
        }
      ]
    },
    {
      "serviceTypeName": "/bmw/ncar/services/TirePressureService",
      "version": {
        "major": 12,
        "minor": 34
      },
      "bindings": [
        {
             "binding": "SHM",
             "serviceId": 1235,
             "events": [],
             "fields": []
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

TEST(ConfigParserDeathTest, NoInstanceSpecifierInInstanceWillDie)
{
    // Given a JSON without necessary attribute `instanceSpecifier`
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
        }
    ],
    "serviceInstances": [
        {
            "serviceTypeName": "/bmw/ncar/services/TirePressureService"
        }
    ]
  }
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, ServiceInstanceReferencesUnknownServiceTypeWillDie)
{
    // Given a JSON, where a service instance references via serviceTypeName an unknown/not configured service type.
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/MeDoesntExist",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                  "asil-level": "QM",
                  "binding": "SHM"
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

TEST(ConfigParserDeathTest, ServiceInstanceEventReferencesUnknownServiceTypeEventWillDie)
{
    // Given a JSON, where a service instance event has a name, which doesn't exist in the serviceType it references.
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
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "eventId": 20
                      }
                  ],
                  "fields": [
                      {
                          "fieldName": "CurrentTemperatureFrontLeft",
                          "fieldId": 30
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
                  "events": [
                    {
                      "eventName": "CurrentPressureFrontLeft"
                    },
                    {
                      "eventName": "Unknown"
                    }
                  ],
                  "fields": []
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

TEST(ConfigParserDeathTest, NoVersionInInstanceWillDie)
{
    // Given a JSON without necessary attribute `version`
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService"
        }
    ]
  }
)"_json;
    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, NoVersionDetailsInInstanceWillDie)
{
    // Given a JSON without necessary attribute `major`
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "minor": 34
            }
        }
    ]
  }
)"_json;
    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, NoDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON without necessary attribute `instances`
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            }
        }
    ]
  }
)"_json;
    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, EmptyDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON without elements in array `instances`.
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
            ]
        }
    ]
  }
)"_json;
    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, UnknownDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON with an unknown binding "HappyHippo" in an instance deployment.
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "asil-level": "QM",
                  "binding": "HappyHippo"
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

TEST(ConfigParserDeathTest, DuplicateServiceInstanceWillDie)
{
    // Given a JSON with two service instances with same instanceSpecifier
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "asil-level": "QM",
                  "binding": "SHM"
                }
            ]
        },
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                  "asil-level": "QM",
                  "binding": "SOME/IP"
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

TEST(ConfigParserDeathTest, NoAsilInDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON without necessary attribute `asil-level`
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "binding": "SHM"
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

TEST(ConfigParserDeathTest, NoBindingInfoInDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON without necessary attribute `binding`
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "asil-level": "QM"
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

TEST_F(ConfigParserFixtureDeathTest, LolaEventWithoutNameCausesTermination)
{
    // Given a JSON without necessary attribute `name` for an event for Shm-Binding Info
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "events": [
                    {}
                  ],
                  "fields": []
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

TEST_F(ConfigParserFixtureDeathTest, LolaFieldWithoutNameCausesTermination)
{
    // Given a JSON without necessary attribute `name` for a field for Shm-Binding Info
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "fields": [
                    {}
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

TEST_F(ConfigParserFixtureDeathTest, LolaEventNameDuplicateCausesTermination)
{
    // Given a JSON where a LoLa event has been duplicated
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
              "events": [
                {
                  "eventName": "CurrentPressureFrontLeft",
                  "eventId": 20
                },
                {
                  "eventName": "CurrentPressureFrontLeft",
                  "eventId": 21
                }
              ],
              "fields": []
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

TEST_F(ConfigParserFixtureDeathTest, LolaEventIdDuplicateCausesTermination)
{
    // Given a JSON where a LoLa event id has been duplicated
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
              "events": [
                {
                  "eventName": "CurrentPressureFrontLeft",
                  "eventId": 20
                },
                {
                  "eventName": "CurrentPressureFrontRight",
                  "eventId": 20
                }
              ],
              "fields": []
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
                  "events": [
                    {
                      "eventName": "CurrentPressureFrontLeft"
                    },
                    {
                      "eventName": "CurrentPressureFrontRight"
                    }
                  ],
                  "fields": []
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

TEST_F(ConfigParserFixtureDeathTest, LolaFieldNameDuplicateCausesTermination)
{
    // Given a JSON where a LoLa field has been duplicated
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
              "fields": [
                {
                  "fieldName": "CurrentPressureFrontLeft",
                  "fieldId": 20
                },
                {
                  "fieldName": "CurrentPressureFrontLeft",
                  "fieldId": 21
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

TEST_F(ConfigParserFixtureDeathTest, LolaFieldIdDuplicateCausesTermination)
{
    // Given a JSON where a LoLa event id has been duplicated
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
              "fields": [
                {
                  "fieldName": "CurrentPressureFrontLeft",
                  "fieldId": 20
                },
                {
                  "fieldName": "CurrentPressureFrontRight",
                  "fieldId": 20
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
                  "fields": [
                    {
                      "fieldName": "CurrentPressureFrontLeft"
                    },
                    {
                      "fieldName": "CurrentPressureFrontRight"
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

TEST_F(ConfigParserFixtureDeathTest, LolaMatchingEventAndFieldIdsIsNotAllowed)
{
    // Given a JSON where a LoLa field has been duplicated
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
              "events": [
                {
                  "eventName": "CurrentPressureFrontLeft",
                  "eventId": 20
                }
              ],
              "fields": [
                {
                  "fieldName": "CurrentPressureFrontRight",
                  "fieldId": 20
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

TEST_F(ConfigParserFixtureDeathTest, LolaIncorrectEventNameCausesTermination)
{
    // Given a JSON where a LoLa event name is incorrect, not 'EventName'
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "events": [
                    {
                      "eventName1": "CurrentPressureFrontLeft"
                    }
                  ],
                  "fields": []
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

TEST_F(ConfigParserFixtureDeathTest, LolaIncorrectFieldNameCausesTermination)
{
    // Given a JSON where a LoLa field name is incorrect, not 'fieldName'
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "fields": [
                    {
                      "fieldName1": "CurrentTemperatureFrontLeft"
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

TEST_F(ConfigParserFixtureDeathTest, LolaEventMaxSamplesAndNumberOfSampleSlotsCausesTermination)
{
    // Given a JSON where a LoLa event has both properties configured maxSamples (deprecated) and numberOfSampleSlots
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "events": [
                    {
                      "eventName": "CurrentPressureFrontLeft",
                      "maxSubscribers": 5,
                      "maxSamples": 7,
                      "numberOfSampleSlots": 7
                    }
                  ],
                  "fields": []
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

TEST(ConfigParser, NoEventMaxSubscribersLeavesValueOptional)
{
    // Given a JSON where a LoLa event has no configured max-subscribers
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
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "eventId": 20
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
                  "events": [
                    {
                      "eventName": "CurrentPressureFrontLeft",
                      "numberOfSampleSlots": 50
                    }
                  ],
                  "fields": []
                }
            ]
        }
    ]
  }
)"_json;
    // When parsing the JSON
    const auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    const auto deployment =
        config.GetServiceInstances().at(InstanceSpecifier::Create("abc/abc/TirePressurePort").value());

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    // That the max_subscribers_ in the event has no value
    EXPECT_FALSE(deploymentInfo.events_.at("CurrentPressureFrontLeft").max_subscribers_.has_value());
}

TEST(ConfigParser, NoFieldMaxSubscribersLeavesValueOptional)
{
    // Given a JSON where a LoLa field has no configured max-subscribers
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
                  "fields": [
                      {
                          "fieldName": "CurrentTemperatureFrontLeft",
                          "fieldId": 20
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
                  "fields": [
                    {
                      "fieldName": "CurrentTemperatureFrontLeft",
                      "numberOfSampleSlots": 50
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

    const auto deployment =
        config.GetServiceInstances().at(InstanceSpecifier::Create("abc/abc/TirePressurePort").value());

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    // That the max_subscribers_ in the field has no value
    EXPECT_FALSE(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").max_subscribers_.has_value());
}

TEST(ConfigParser, NoSHMInstanceIdLeavesValueOptional)
{
    // Given a JSON without necessary attribute `instance_id_` for SHM-Binding Info
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "serviceId": 1234,
                  "asil-level": "QM",
                  "binding": "SHM"
                }
            ]
        }
    ]
  }
)"_json;
    const auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    const auto deployment =
        config.GetServiceInstances().at(InstanceSpecifier::Create("abc/abc/TirePressurePort").value());

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    ASSERT_FALSE(deploymentInfo.instance_id_.has_value());
}

TEST(ConfigParser, LolaEventOptionalMaxConcurrentAllocations)
{
    // Given a JSON with an event with optional max concurrent allocations set
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
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "eventId": 20
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
                  "serviceId": 1234,
                  "asil-level": "QM",
                  "binding": "SHM",
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "numberOfSampleSlots": 50,
                          "maxSubscribers": 5,
                          "maxConcurrentAllocations": 2
                      }
                  ],
                  "fields": []
                }
            ]
        }
    ]
  }
)"_json;

    // When parsing such a configuration
    // Fail and abort
    EXPECT_EXIT(score::mw::com::impl::configuration::Parse(std::move(j2)), ::testing::KilledBySignal(SIGABRT), ".*");
}

TEST(ConfigParser, LolaEventDeprecatedMaxSamplesGetsRecognized)
{
    // Given a JSON with an event with deprecated maxSamples property is still recognized
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
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "eventId": 20
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
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "maxSamples": 50,
                          "maxSubscribers": 5
                      }
                  ],
                  "fields": []
                }
            ]
        }
    ]
  }
)"_json;
    const auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    const auto deployment =
        config.GetServiceInstances().at(InstanceSpecifier::Create("abc/abc/TirePressurePort").value());

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    EXPECT_EQ(deploymentInfo.events_.at("CurrentPressureFrontLeft").GetNumberOfSampleSlots().value(), 50);
}

TEST(ConfigParser, LolaFieldOptionalMaxConcurrentAllocations)
{
    // Given a JSON with a field with optional max concurrent allocations set
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
                  "fields": [
                      {
                          "fieldName": "CurrentTemperatureFrontLeft",
                          "fieldId": 20
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
                  "fields": [
                    {
                          "fieldName": "CurrentTemperatureFrontLeft",
                          "numberOfSampleSlots": 50,
                          "maxSubscribers": 5,
                          "maxConcurrentAllocations": 2
                      }
                  ]
                }
            ]
        }
    ]
  }
)"_json;
    // When parsing such a configuration
    // Fail and abort
    EXPECT_EXIT(score::mw::com::impl::configuration::Parse(std::move(j2)), ::testing::KilledBySignal(SIGABRT), ".*");
}

TEST(ConfigParser, LolaEventOptionalEnforceMaxSamples)
{
    RecordProperty("Verifies", "SCR-7088394");
    RecordProperty("Description", "Checks whether optional 'enforceMaxSamples' configuration can be read at runtime");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with optional attribute `enforceMaxSamples` for SHM-Binding Info
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
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "eventId": 20
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
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "numberOfSampleSlots": 50,
                          "maxSubscribers": 5,
                          "enforceMaxSamples": false
                      }
                  ],
                  "fields": []
                }
            ]
        }
    ]
  }
)"_json;
    const auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    const auto deployment =
        config.GetServiceInstances().at(InstanceSpecifier::Create("abc/abc/TirePressurePort").value());

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    EXPECT_EQ(deploymentInfo.events_.at("CurrentPressureFrontLeft").enforce_max_samples_.value(), false);
}

TEST(ConfigParser, LolaFieldOptionalEnforceMaxSamples)
{
    // Given a JSON with optional attribute `enforceMaxSamples` for SHM-Binding Info
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
                  "fields": [
                      {
                          "fieldName": "CurrentTemperatureFrontLeft",
                          "fieldId": 20
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
                  "fields": [
                    {
                          "fieldName": "CurrentTemperatureFrontLeft",
                          "numberOfSampleSlots": 50,
                          "maxSubscribers": 5,
                          "enforceMaxSamples": false
                      }
                  ]
                }
            ]
        }
    ]
  }
)"_json;
    const auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    const auto deployment =
        config.GetServiceInstances().at(InstanceSpecifier::Create("abc/abc/TirePressurePort").value());

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    EXPECT_EQ(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").enforce_max_samples_.value(), false);
}

TEST(ConfigParser, EmptyServiceTypes)
{
    // Given a JSON with necessary attribute `serviceTypes` being empty (which is allowed)
    auto j2 = R"(
  {
    "serviceInstances": [],
    "serviceTypes": []
  }
)"_json;
    // When parsing the JSON
    // That the application will terminate
    Configuration config{score::mw::com::impl::configuration::Parse(std::move(j2))};
    EXPECT_EQ(config.GetServiceTypes().size(), 0);
}

TEST(ConfigParser, StrictPermissionIsSet)
{
    // Given a JSON with `permission-checks` attribute which is set to `strict`
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
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "eventId": 20
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
                  "events": [
                    {
                      "eventName": "CurrentPressureFrontLeft",
                      "numberOfSampleSlots": 50
                    }
                  ],
                  "permission-checks": "strict",
                  "fields": []
                }
            ]
        }
    ]
  }
)"_json;
    // When parsing the JSON
    const auto configuration = score::mw::com::impl::configuration::Parse(std::move(j2));
    const auto instances = configuration.GetServiceInstances();
    ASSERT_FALSE(instances.empty());

    // That LolaServiceInstanceDeployment instance is obtained
    const auto deployment = instances.begin()->second;
    const auto* const lola_service_instance = std::get_if<LolaServiceInstanceDeployment>(&deployment.bindingInfo_);
    ASSERT_NE(lola_service_instance, nullptr);
    // And "permission-checks" attribute is set to "strict"
    EXPECT_TRUE(lola_service_instance->strict_permissions_);
}

TEST(ConfigParser, GetNoneStrictIfNoPermissionFlagAttr)
{
    // Given a JSON without `permission-checks` attribute
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
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "eventId": 20
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
                  "events": [
                    {
                      "eventName": "CurrentPressureFrontLeft",
                      "numberOfSampleSlots": 50
                    }
                  ],
                  "fields": []
                }
            ]
        }
    ]
  }
)"_json;
    // When parsing the JSON
    const auto configuration = score::mw::com::impl::configuration::Parse(std::move(j2));
    const auto instances = configuration.GetServiceInstances();
    ASSERT_FALSE(instances.empty());

    // That LolaServiceInstanceDeployment instance is obtained
    const auto deployment = instances.begin()->second;
    const auto* const lola_service_instance = std::get_if<LolaServiceInstanceDeployment>(&deployment.bindingInfo_);
    ASSERT_NE(lola_service_instance, nullptr);
    // And "permission-checks" attribute is set to none-"strict"
    EXPECT_FALSE(lola_service_instance->strict_permissions_);
}

class ProcessAsil : public ::testing::TestWithParam<std::tuple<std::string, QualityType>>
{
};

TEST_P(ProcessAsil, ValidProcessAsilLevel)
{
    json::JsonParser json_parser_obj;
    json::Any json{json_parser_obj.FromBuffer(std::get<std::string>(GetParam())).value()};
    Configuration config{configuration::Parse(std::move(json))};
    EXPECT_EQ(config.GetGlobalConfiguration().GetProcessAsilLevel(), std::get<QualityType>(GetParam()));
}

const std::vector<std::tuple<std::string, QualityType>> valid_global_asil{
    {R"json({"serviceTypes": [], "serviceInstances": [], "global": { "asil-level": "QM" }})json",
     QualityType::kASIL_QM},
    {R"json({"serviceTypes": [], "serviceInstances": [], "global": { "asil-level": "B" }})json", QualityType::kASIL_B},
    {R"json({"serviceTypes": [], "serviceInstances": [] })json", QualityType::kASIL_QM},
};

INSTANTIATE_TEST_SUITE_P(ValidProcessAsil, ProcessAsil, ::testing::ValuesIn(valid_global_asil));

class InvalidProcessAsil : public ::testing::TestWithParam<std::string>
{
};

TEST_P(InvalidProcessAsil, DieOnInvalidAsil)
{
    score::json::JsonParser json_parser_obj;
    json::Any json{json_parser_obj.FromBuffer(GetParam()).value()};
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"  // Comming from gtest, try removing when gtest 1.12 or higher
#endif
    EXPECT_DEATH(Configuration{configuration::Parse(std::move(json))}, ".*");
#if defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

std::string inconsistent_asil_config = R"json(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "asil-level": "B",
                  "binding": "SHM",
                  "events": [],
                  "fields": []
                }
            ]
        }
    ],
    "global": {
        "asil-level": "QM"
    }
  }
)json";

INSTANTIATE_TEST_SUITE_P(
    InvalidProcessAsil,
    InvalidProcessAsil,
    ::testing::Values(R"json({"serviceTypes": [], "serviceInstances": [], "global": { "asil-level": "ANY" }})json",
                      R"json({"serviceTypes": [], "serviceInstances": [], "global": { "asil-level": "Elefant" }})json",
                      R"json({"serviceTypes": [], "serviceInstances": [], "global": { "asil-level": "" }})json",
                      inconsistent_asil_config));

class InvalidMsgQueueSizeFixture : public ::testing::TestWithParam<std::string>
{
};

TEST_P(InvalidMsgQueueSizeFixture, DieOnInvalidMessageQueueSize)
{
    score::json::JsonParser json_parser_obj;
    json::Any json{json_parser_obj.FromBuffer(GetParam()).value()};
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"  // Comming from gtest, try removing when gtest 1.12 or higher
#endif
    EXPECT_DEATH(Configuration{configuration::Parse(std::move(json))}, ".*");
#if defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

INSTANTIATE_TEST_SUITE_P(
    InvalidMsgQueueSizeTests,
    InvalidMsgQueueSizeFixture,
    ::testing::Values(
        R"json({"serviceTypes": [], "serviceInstances": [], "global": { "asil-level": "B", "queue-size": {"QM-receiver": 8, "B-receiver": "bla"}}})json",
        R"json({"serviceTypes": [], "serviceInstances": [], "global": { "asil-level": "B", "queue-size": {"QM-receiver": 8, "B-receiver": "bla", "B-sender": 15}}})json",
        R"json({"serviceTypes": [], "serviceInstances": [], "global": { "asil-level": "B", "queue-size": {"QM-receiver": 8, "B-receiver": 5, "B-sender": "bla"}}})json",
        R"json({"serviceTypes": [], "serviceInstances": [], "global": { "asil-level": "B", "queue-size": {"QM-receiver": "bla", "B-receiver": 9}}})json"));

TEST(ConfigParser, OnlyQmReceiverQueueSizes)
{
    // Given a JSON with only QM-receiver queue size being explicitly configured
    auto j2 = R"(
  {
    "serviceTypes": [],
    "serviceInstances": [],
    "global": {
       "queue-size": {
          "QM-receiver": 8
      }
    }
  }
)"_json;
    // When parsing the JSON
    const auto config = score::mw::com::impl::configuration::Parse(std::move(j2));
    // expect that the QM-receiver has the configured value
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_QM), 8);
    // and that the not explicit configured B-receiver has the default value (DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE)
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_B),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE);
    // and that the not explicit configured B-sender has the default value (DEFAULT_MIN_NUM_MESSAGES_TX_QUEUE)
    EXPECT_EQ(config.GetGlobalConfiguration().GetSenderMessageQueueSize(),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_TX_QUEUE);
}

TEST(ConfigParser, WrongQualityTypeForAllowedUsersWillDie)
{
    // Given a JSON without necessary attribute `instance_id_` for SHM-Binding Info
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "asil-level": "B",
                  "binding": "SHM",
                  "shm-size": 10000,
                  "allowedConsumer": {
                    "QM": [
                      42,
                      43
                    ]
                  }
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

TEST(ConfigParser, InvalidQualityTypeForAllowedConsumersWillDie)
{
    // Given a JSON without invalid attribute consumer quality type
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
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
                  "asil-level": "QM",
                  "binding": "SHM",
                  "allowedConsumer": {
                    "INVALID_QUALITY_TYPE": [
                      42,
                      43
                    ]
                  }
                }
            ]
        }
    ]
  }
)"_json;
    // When parsing the JSON
    // Then the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParser, TerminateOnParsingSomeIP)
{
    // Give a SOME/IP binding configuration
    auto config_with_some_ip_binding = R"(
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
                "serviceId": 1234,
                "binding": "SOME/IP",
                "events": [],
                "fields": []
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
                    "instanceId": 1,
                    "asil-level": "B",
                    "binding": "SOME/IP",
                    "events": [],
                    "fields": []
                    }
                ]
            }
        ],
        "global": {
        "asil-level": "B"
        }
    }
    )";
    const score::json::JsonParser json_parser_obj;
    auto json = json_parser_obj.FromBuffer(config_with_some_ip_binding);

    // When parsing such a configuration
    // Fail and abort
    EXPECT_EXIT(configuration::Parse(std::move(json).value()), ::testing::KilledBySignal(SIGABRT), ".*");
}

class ShmSizeCalcMode : public ::testing::TestWithParam<std::tuple<std::string, ShmSizeCalculationMode>>
{
};

TEST_P(ShmSizeCalcMode, ValidShmSizeCalcMode)
{
    json::JsonParser json_parser_obj;
    json::Any json{json_parser_obj.FromBuffer(std::get<std::string>(GetParam())).value()};
    Configuration config{configuration::Parse(std::move(json))};
    EXPECT_EQ(config.GetGlobalConfiguration().GetShmSizeCalcMode(), std::get<ShmSizeCalculationMode>(GetParam()));
}

const std::vector<std::tuple<std::string, ShmSizeCalculationMode>> valid_global_shm_size_calc_modes{
    {R"json({"serviceTypes": [], "serviceInstances": [], "global": { "shm-size-calc-mode": "SIMULATION" }})json",
     ShmSizeCalculationMode::kSimulation},
    {R"json({"serviceTypes": [], "serviceInstances": [] })json", ShmSizeCalculationMode::kSimulation},
};

INSTANTIATE_TEST_SUITE_P(ValidShmSizeCalcMode, ShmSizeCalcMode, ::testing::ValuesIn(valid_global_shm_size_calc_modes));

TEST(ConfigParserTracing, EnablingGlobalTracingFlagSetsTracingEnabled)
{
    RecordProperty("Verifies", "SCR-18159733");
    RecordProperty("Description",
                   "TracingConfiguration IsTracingEnabled is true when global tracing enabled flag is set.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with the global tracing flag enabled
    auto j2 = R"(
  {
    "serviceInstances": [],
    "serviceTypes": [],
    "tracing": {
        "enable": true,
        "applicationInstanceID": "test_application_id",
        "traceFilterConfigPath": "./test_filter_config.json"
    }
  }
)"_json;
    // When parsing the JSON
    Configuration config{score::mw::com::impl::configuration::Parse(std::move(j2))};

    // Then tracing is enabled in the TracingConfiguration
    EXPECT_TRUE(config.GetTracingConfiguration().IsTracingEnabled());
}

TEST(ConfigParserTracing, EnablingGlobalTracingFlagSetsTracingDisbled)
{
    RecordProperty("Verifies", "SCR-18159733");
    RecordProperty("Description",
                   "TracingConfiguration IsTracingEnabled is false when global tracing enabled flag is not set.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with the global tracing flag disabled
    auto j2 = R"(
  {
    "serviceInstances": [],
    "serviceTypes": [],
    "tracing": {
        "enable": false,
        "applicationInstanceID": "test_application_id",
        "traceFilterConfigPath": "./test_filter_config.json"
    }
  }
)"_json;
    // When parsing the JSON
    Configuration config{score::mw::com::impl::configuration::Parse(std::move(j2))};

    // Then tracing is disabled in the TracingConfiguration
    EXPECT_FALSE(config.GetTracingConfiguration().IsTracingEnabled());
}

TEST(ConfigParserTracing, ProvidingAllTracingConfigElementsDoesNotCrash)
{
    RecordProperty("Verifies", "SCR-18143152");
    RecordProperty("Description", "mw/com configuration file contains flag for enabling / disabling tracing.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with all tracing attributes
    auto j2 = R"(
  {
    "serviceInstances": [],
    "serviceTypes": [],
    "tracing": {
        "enable": false,
        "applicationInstanceID": "test_application_id",
        "traceFilterConfigPath": "./test_filter_config.json"
    }
  }
)"_json;
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{score::mw::com::impl::configuration::Parse(std::move(j2))};

    EXPECT_FALSE(config.GetTracingConfiguration().IsTracingEnabled());
    EXPECT_EQ(config.GetTracingConfiguration().GetApplicationInstanceID(), score::cpp::string_view{"test_application_id"});
    EXPECT_EQ(config.GetTracingConfiguration().GetTracingFilterConfigPath(),
              score::cpp::string_view{"./test_filter_config.json"});
}

TEST(ConfigParserTracing, ProvidingAllRequiredTracingConfigElementsDoesNotCrash)
{
    RecordProperty("Verifies", "SCR-18143152, SCR-18143480");
    RecordProperty("Description",
                   "Flag for enabling / disabling tracing is optional (SCR-18143152). If the flag is not provided, it "
                   "will default to false (SCR-18143480).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with all tracing attributes
    auto j2 = R"(
  {
    "serviceInstances": [],
    "serviceTypes": [],
    "tracing": {
        "applicationInstanceID": "test_application_id"
    }
  }
)"_json;
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{score::mw::com::impl::configuration::Parse(std::move(j2))};

    EXPECT_FALSE(config.GetTracingConfiguration().IsTracingEnabled());
    EXPECT_EQ(config.GetTracingConfiguration().GetApplicationInstanceID(), score::cpp::string_view{"test_application_id"});
}

TEST(ConfigParserTracing, ParsingSucceedsIfApplicationInstanceIdentifierPropertyExistsWhenTracingIsEnabled)
{
    RecordProperty("Verifies", "SCR-19177359");
    RecordProperty(
        "Description",
        "Checks that parsing succeeds if applicationInstanceID property is provided when tracing is enabled.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with all tracing attributes
    auto j2 = R"(
  {
    "serviceInstances": [],
    "serviceTypes": [],
    "tracing": {
        "enable": true,
        "traceFilterConfigPath": "./mw_com_trace_filter.json",
        "applicationInstanceID": "test_application_id"
    }
  }
)"_json;
    // When parsing the JSON
    // That the application will not terminate
    score::cpp::ignore = score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST(ConfigParserTracing, ParsingTerminatesIfApplicationInstanceIdentifierPropertyDoesNotExistWhenTracingIsEnabled)
{
    RecordProperty("Verifies", "SCR-19177359");
    RecordProperty(
        "Description",
        "Checks that parsing terminates if applicationInstanceID property is not provided when tracing is enabled.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON which is missing applicationInstanceID
    auto j2 = R"(
  {
    "serviceInstances": [],
    "serviceTypes": [],
    "tracing": {
        "enable": true,
        "traceFilterConfigPath": "./mw_com_trace_filter.json"
    }
  }
)"_json;
    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserTracing, ParsingSucceedsIfTraceFilterConfigPathPropertyExistsWhenTracingSectionIsPresent)
{
    RecordProperty("Verifies", "SCR-18144291");
    RecordProperty("Description",
                   "Checks that parsing succeeds if traceFilterConfigPath property is provided when the tracing "
                   "section is present in the configuration file.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with all tracing attributes
    auto j2 = R"(
  {
    "serviceInstances": [],
    "serviceTypes": [],
    "tracing": {
        "enable": true,
        "traceFilterConfigPath": "./mw_com_trace_filter.json",
        "applicationInstanceID": "test_application_id"
    }
  }
)"_json;
    // When parsing the JSON
    // That the application will not terminate
    score::cpp::ignore = score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST(ConfigParserTracing, ParsingSucceedsIfTraceFilterConfigPathPropertyDoesNotExistWhenTracingSectionIsPresent)
{
    RecordProperty("Verifies", "SCR-18144411");
    RecordProperty("Description",
                   "Checks that the traceFilterConfigPath property is optional and the default value is "
                   "./etc/mw_com_trace_filter.json.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with all tracing attributes except for traceFilterConfigPath
    auto j2 = R"(
  {
    "serviceInstances": [],
    "serviceTypes": [],
    "tracing": {
        "enable": true,
        "applicationInstanceID": "test_application_id"
    }
  }
)"_json;
    // When parsing the JSON
    // That the application will not terminate
    const auto config{score::mw::com::impl::configuration::Parse(std::move(j2))};

    EXPECT_EQ(config.GetTracingConfiguration().GetTracingFilterConfigPath(),
              score::cpp::string_view{"./etc/mw_com_trace_filter.json"});
}

TEST(ConfigParserTracing, ProvidingServiceElementEnabledEnablesServiceElementTracing)
{
    // Given a JSON with all tracing attributes
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
                "events": [
                    {
                        "eventName": "CurrentPressureFrontLeft",
                        "eventId": 20
                    }
                ],
                "fields": [
                    {
                        "fieldName": "CurrentPressureFrontRight",
                        "fieldId": 30
                    }
                ]
            }
          ]
        },
        {
          "serviceTypeName": "/bmw/ncar/services/TireTemperatureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": [
            {
                "binding": "SHM",
                "serviceId": 1235,
                "events": [
                    {
                        "eventName": "CurrentTemperatureFrontLeft",
                        "eventId": 20
                    }
                ],
                "fields": [
                    {
                        "fieldName": "CurrentTemperatureFrontRight",
                        "fieldId": 30
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
                  "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "maxSamples": 50,
                            "maxSubscribers": 5,
                            "numberOfIpcTracingSlots": 0
                        }
                    ],
                    "fields": [
                        {
                            "fieldName": "CurrentPressureFrontRight",
                            "numberOfSampleSlots": 60,
                            "maxSubscribers": 6,
                            "numberOfIpcTracingSlots": 1
                        }
                    ]
                }
            ]
        },
        {
            "instanceSpecifier": "abc/abc/TireTemperaturePort",
            "serviceTypeName": "/bmw/ncar/services/TireTemperatureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                  "instanceId": 4567,
                  "asil-level": "QM",
                  "binding": "SHM",
                  "events": [
                        {
                            "eventName": "CurrentTemperatureFrontLeft",
                            "maxSamples": 50,
                            "maxSubscribers": 5,
                            "numberOfIpcTracingSlots": 1
                        }
                    ],
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontRight",
                            "numberOfSampleSlots": 60,
                            "maxSubscribers": 6,
                            "numberOfIpcTracingSlots": 1
                        }
                    ]
                }
            ]
        }
    ],
    "tracing": {
        "enable": true,
        "applicationInstanceID": "test_application_id"
    }
  }
)"_json;
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{score::mw::com::impl::configuration::Parse(std::move(j2))};
    const auto& tracing_config = config.GetTracingConfiguration();
    EXPECT_TRUE(tracing_config.IsTracingEnabled());

    tracing::ServiceElementIdentifierView service_1_event{
        "/bmw/ncar/services/TirePressureService", "CurrentPressureFrontLeft", tracing::ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_1_field{
        "/bmw/ncar/services/TirePressureService", "CurrentPressureFrontRight", tracing::ServiceElementType::FIELD};

    tracing::ServiceElementIdentifierView service_2_event{
        "/bmw/ncar/services/TireTemperatureService", "CurrentTemperatureFrontLeft", tracing::ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_2_field{"/bmw/ncar/services/TireTemperatureService",
                                                          "CurrentTemperatureFrontRight",
                                                          tracing::ServiceElementType::FIELD};

    const auto service_1_instance_specifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
    const auto service_2_instance_specifier = InstanceSpecifier::Create("abc/abc/TireTemperaturePort").value();
    const auto service_1_instance_specifier_std_string_view = service_1_instance_specifier.ToString();
    const auto service_2_instance_specifier_std_string_view = service_2_instance_specifier.ToString();
    const auto service_1_instance_specifier_string_view = score::cpp::string_view{
        service_1_instance_specifier_std_string_view.data(), service_1_instance_specifier_std_string_view.size()};
    const auto service_2_instance_specifier_string_view = score::cpp::string_view{
        service_2_instance_specifier_std_string_view.data(), service_2_instance_specifier_std_string_view.size()};

    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_event, service_1_instance_specifier_string_view));
    EXPECT_TRUE(
        tracing_config.IsServiceElementTracingEnabled(service_1_field, service_1_instance_specifier_string_view));
    EXPECT_TRUE(
        tracing_config.IsServiceElementTracingEnabled(service_2_event, service_2_instance_specifier_string_view));
    EXPECT_TRUE(
        tracing_config.IsServiceElementTracingEnabled(service_2_field, service_2_instance_specifier_string_view));
}

TEST(ConfigParserTracing, DisablingGlobalTracingReturnsFalseForAllCallsToIsServiceElementTracingEnabled)
{
    // Given a JSON with all tracing attributes
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
                "events": [
                    {
                        "eventName": "CurrentPressureFrontLeft",
                        "eventId": 20
                    }
                ],
                "fields": [
                    {
                        "fieldName": "CurrentPressureFrontRight",
                        "fieldId": 30
                    }
                ]
            }
          ]
        },
        {
          "serviceTypeName": "/bmw/ncar/services/TireTemperatureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": [
            {
                "binding": "SHM",
                "serviceId": 1235,
                "events": [
                    {
                        "eventName": "CurrentTemperatureFrontLeft",
                        "eventId": 20
                    }
                ],
                "fields": [
                    {
                        "fieldName": "CurrentTemperatureFrontRight",
                        "fieldId": 30
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
                  "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "maxSamples": 50,
                            "maxSubscribers": 5,
                            "numberOfIpcTracingSlots": 0
                        }
                    ],
                    "fields": [
                        {
                            "fieldName": "CurrentPressureFrontRight",
                            "numberOfSampleSlots": 60,
                            "maxSubscribers": 6,
                            "numberOfIpcTracingSlots": 1
                        }
                    ]
                }
            ]
        },
        {
            "instanceSpecifier": "abc/abc/TireTemperaturePort",
            "serviceTypeName": "/bmw/ncar/services/TireTemperatureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                  "instanceId": 4567,
                  "asil-level": "QM",
                  "binding": "SHM",
                  "events": [
                        {
                            "eventName": "CurrentTemperatureFrontLeft",
                            "maxSamples": 50,
                            "maxSubscribers": 5,
                            "numberOfIpcTracingSlots": 1
                        }
                    ],
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontRight",
                            "numberOfSampleSlots": 60,
                            "maxSubscribers": 6,
                            "numberOfIpcTracingSlots": 1
                        }
                    ]
                }
            ]
        }
    ],
    "tracing": {
        "enable": false,
        "applicationInstanceID": "test_application_id"
    }
  }
)"_json;
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{score::mw::com::impl::configuration::Parse(std::move(j2))};
    const auto& tracing_config = config.GetTracingConfiguration();
    EXPECT_FALSE(tracing_config.IsTracingEnabled());

    tracing::ServiceElementIdentifierView service_1_event{
        "/bmw/ncar/services/TirePressureService", "CurrentPressureFrontLeft", tracing::ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_1_field{
        "/bmw/ncar/services/TirePressureService", "CurrentPressureFrontRight", tracing::ServiceElementType::FIELD};

    tracing::ServiceElementIdentifierView service_2_event{
        "/bmw/ncar/services/TireTemperatureService", "CurrentTemperatureFrontLeft", tracing::ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_2_field{"/bmw/ncar/services/TireTemperatureService",
                                                          "CurrentTemperatureFrontRight",
                                                          tracing::ServiceElementType::FIELD};

    const auto service_1_instance_specifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
    const auto service_2_instance_specifier = InstanceSpecifier::Create("abc/abc/TireTemperaturePort").value();
    const auto service_1_instance_specifier_std_string_view = service_1_instance_specifier.ToString();
    const auto service_2_instance_specifier_std_string_view = service_2_instance_specifier.ToString();
    const auto service_1_instance_specifier_string_view = score::cpp::string_view{
        service_1_instance_specifier_std_string_view.data(), service_1_instance_specifier_std_string_view.size()};
    const auto service_2_instance_specifier_string_view = score::cpp::string_view{
        service_2_instance_specifier_std_string_view.data(), service_2_instance_specifier_std_string_view.size()};

    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_event, service_1_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_field, service_1_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_2_event, service_2_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_2_field, service_2_instance_specifier_string_view));
}

TEST(ConfigParserTracing, NotProvidingServiceElementEnabledDisablesServiceElementTracing)
{
    // Given a JSON with all tracing attributes
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
                "events": [
                    {
                        "eventName": "CurrentPressureFrontLeft",
                        "eventId": 20
                    }
                ],
                "fields": [
                    {
                        "fieldName": "CurrentPressureFrontRight",
                        "fieldId": 30
                    }
                ]
            }
          ]
        },
        {
          "serviceTypeName": "/bmw/ncar/services/TireTemperatureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": [
            {
                "binding": "SHM",
                "serviceId": 1235,
                "events": [
                    {
                        "eventName": "CurrentTemperatureFrontLeft",
                        "eventId": 20
                    }
                ],
                "fields": [
                    {
                        "fieldName": "CurrentTemperatureFrontRight",
                        "fieldId": 30
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
                  "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "maxSamples": 50,
                            "maxSubscribers": 5,
                            "numberOfIpcTracingSlots": 0
                        }
                    ],
                    "fields": [
                        {
                            "fieldName": "CurrentPressureFrontRight",
                            "numberOfSampleSlots": 60,
                            "maxSubscribers": 6
                        }
                    ]
                }
            ]
        },
        {
            "instanceSpecifier": "abc/abc/TireTemperaturePort",
            "serviceTypeName": "/bmw/ncar/services/TireTemperatureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                  "instanceId": 4567,
                  "asil-level": "QM",
                  "binding": "SHM",
                  "events": [
                        {
                            "eventName": "CurrentTemperatureFrontLeft",
                            "maxSamples": 50,
                            "maxSubscribers": 5,
                            "numberOfIpcTracingSlots": 1
                        }
                    ],
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontRight",
                            "numberOfSampleSlots": 60,
                            "maxSubscribers": 6
                        }
                    ]
                }
            ]
        }
    ],
    "tracing": {
        "enable": true,
        "applicationInstanceID": "test_application_id"
    }
  }
)"_json;
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{score::mw::com::impl::configuration::Parse(std::move(j2))};
    const auto& tracing_config = config.GetTracingConfiguration();
    EXPECT_TRUE(tracing_config.IsTracingEnabled());

    tracing::ServiceElementIdentifierView service_1_event{
        "/bmw/ncar/services/TirePressureService", "CurrentPressureFrontLeft", tracing::ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_1_field{
        "/bmw/ncar/services/TirePressureService", "CurrentPressureFrontRight", tracing::ServiceElementType::FIELD};

    tracing::ServiceElementIdentifierView service_2_event{
        "/bmw/ncar/services/TireTemperatureService", "CurrentTemperatureFrontLeft", tracing::ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_2_field{"/bmw/ncar/services/TireTemperatureService",
                                                          "CurrentTemperatureFrontRight",
                                                          tracing::ServiceElementType::FIELD};

    const auto service_1_instance_specifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
    const auto service_2_instance_specifier = InstanceSpecifier::Create("abc/abc/TireTemperaturePort").value();
    const auto service_1_instance_specifier_std_string_view = service_1_instance_specifier.ToString();
    const auto service_2_instance_specifier_std_string_view = service_2_instance_specifier.ToString();
    const auto service_1_instance_specifier_string_view = score::cpp::string_view{
        service_1_instance_specifier_std_string_view.data(), service_1_instance_specifier_std_string_view.size()};
    const auto service_2_instance_specifier_string_view = score::cpp::string_view{
        service_2_instance_specifier_std_string_view.data(), service_2_instance_specifier_std_string_view.size()};

    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_event, service_1_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_field, service_1_instance_specifier_string_view));
    EXPECT_TRUE(
        tracing_config.IsServiceElementTracingEnabled(service_2_event, service_2_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_2_field, service_2_instance_specifier_string_view));
}

score::json::Any generate_config_json(const std::string& instance_specifier,
                                    const std::string& field_name,
                                    const std::string& number_of_tracing_slots)
{
    std::stringstream config_json_strstr;
    config_json_strstr << R"(
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
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "fieldId": 30
                        }
                    ]
                }
            ]
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": ")"
                       << instance_specifier << R"(",
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
                    "shm-size": 10000,
                    "events": [
                    ],
                    "fields": [
                        {
                            "fieldName": ")"
                       << field_name << R"(",
                            "numberOfSampleSlots": 0,
                            "maxSubscribers": 6,
                            "numberOfIpcTracingSlots": )"
                       << number_of_tracing_slots << R"(
                        }
                    ]
                }
            ]
        }
    ]
}

)";
    auto config_json = operator""_json(config_json_strstr.str().data(), config_json_strstr.str().size());
    return config_json;
}

TEST(TracingFilterConfigGetNumberOfTraceingSlots, CorrectlyParseAJsonContainingNumberOfTracingSlotsInRange)
{
    const std::string instance_specifier_str{"abc/abc/TirePressurePort"};
    const std::string field_name_str{"CurrentTemperatureFrontLeft"};
    constexpr std::uint8_t number_of_tracing_slots{255};

    // Given a config_json containing a numberOfSampleSlots which fits in its capacity
    auto config_json = generate_config_json(
        instance_specifier_str, field_name_str, std::to_string(static_cast<int>(number_of_tracing_slots)));

    // When json is parsed into the configuration
    auto config = score::mw::com::impl::configuration::Parse(std::move(config_json));

    auto serv_inst_depls = config.GetServiceInstances();
    const auto instance_specifier = InstanceSpecifier::Create(instance_specifier_str).value();
    const auto serv_inst_depl_it = serv_inst_depls.at(instance_specifier);
    const auto lola_service_instance_depl = std::get<0>(serv_inst_depl_it.bindingInfo_);
    const auto& field = lola_service_instance_depl.fields_.at(field_name_str);

    // Then the retreived Number of tracing slots matches the original number.
    EXPECT_EQ(number_of_tracing_slots, field.GetNumberOfTracingSlots());
}

TEST(TracingFilterConfigGetNumberOfTraceingSlots, FailParsingAJsonContainingNumberOfTracingSlotsOutOfRange)
{
    const std::string instance_specifier_str{"abc/abc/TirePressurePort"};
    const std::string field_name_str{"CurrentTemperatureFrontLeft"};

    // Given a config_json containing a numberOfSampleSlots which does not fit in its capacity, but is otherwise valid
    auto config_json = generate_config_json(instance_specifier_str, field_name_str, "256");

    // Then Expect parsing to fail
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(config_json)), ".*");
}

TEST(ConfigParserDeathTest, DuplicateServiceInstanceEventsWillDie)
{
    // Given a JSON with duplicate event
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 30
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
                   "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
                        },
                        {
                            "eventName": "CurrentPressureFrontLeft"
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

TEST(ConfigParserDeathTest, NoDuplicateServiceInstanceEventsWillNotDie)
{
    // configuration is the same as the test above and is testing the positive case.
    // Given a JSON without duplicate event
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 30
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
                   "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
                        }
                    ]
                }
            ]
        }
    ]
}

)"_json;

    // When parsing the JSON
    // That the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST(ConfigParserDeathTest, DuplicateServiceInstanceFieldWillDie)
{
    // Given a JSON with duplicate Field
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
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "fieldId": 30
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
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft"
                        },
                        {
                            "fieldName": "CurrentTemperatureFrontLeft"
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

TEST(ConfigParserDeathTest, NoDuplicateServiceInstanceFieldWillNotDie)
{
    // configuration is the same as the test above and is testing the positive case.
    // Given a JSON without duplicate Field
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
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "fieldId": 30
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
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft"
                        }
                    ]
                }
            ]
        }
    ]
}

)"_json;

    // When parsing the JSON
    // That the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST(ConfigParserDeathTest, SpecifyingServiceInstanceFieldWhichDoesNotCorrespondToAServiceTypeFieldWillDie)
{
    // Given a JSON with unknown field
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
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "fieldId": 30
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
                    "fields": [
                        {
                            "fieldName": "Unknown"
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

TEST(ConfigParserDeathTest, SpecifyingServiceInstanceFieldWhichCorrespondToAServiceTypeFieldWillNotDie)
{
    // configuration is the same as the test above and is testing the positive case.
    // Given a JSON with known field
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
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "fieldId": 30
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
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft"
                        }
                    ]
                }
            ]
        }
    ]
}
)"_json;

    // When parsing the JSON
    // That the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST(ConfigParserDeathTest, UnknownShmSizeCalcModeKeyWillDie)
{
    // Given a JSON with invalid shm size calcMode key
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
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
                    "asil-level": "B",
                    "binding": "SHM",
                    "shm-size": 10000,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
                        }
                    ]
                }
            ]
        }
    ],
    "global": {
        "asil-level": "B",
        "queue-size": {
            "QM-receiver": 8,
            "B-receiver": 5,
            "B-sender": 12
        },
        "shm-size-calc-mode": "Unknown"
    }
}
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, KnownShmSizeCalcModeKeyWillNotDie)
{
    // configuration is the same as the test above and is testing the positive case.
    // Given a JSON with valid shm size calcMode key
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
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
                    "asil-level": "B",
                    "binding": "SHM",
                    "shm-size": 10000,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
                        }
                    ]
                }
            ]
        }
    ],
    "global": {
        "asil-level": "B",
        "queue-size": {
            "QM-receiver": 8,
            "B-receiver": 5,
            "B-sender": 12
        },
        "shm-size-calc-mode": "SIMULATION"
    }
}
)"_json;

    // When parsing the JSON
    // That the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST(ConfigParserDeathTest, WithoutServiceinstancesWillDie)
{
    // Given a JSON without necessary service instance
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": []
        }
    ]
  }
)"_json;

    // When parsing the JSON
    // That the application will terminate
    EXPECT_DEATH(score::mw::com::impl::configuration::Parse(std::move(j2)), ".*");
}

TEST(ConfigParserDeathTest, WithServiceinstancesWillNotDie)
{
    // configuration is the same as the test above and is testing the positive case.
    // Given a JSON with necessary service instance
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

            ]
        }
    ],
    "serviceInstances": [
    ]
}

)"_json;

    // When parsing the JSON
    // That the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST(ConfigParserDeathTest, EmptyInstanceSpecifierWillDie)
{
    // configuration is the same as the test below and is testing the positive case.
    // Given a JSON with empty instance specifier
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
                        }
                    ]
                }
            ]
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "",
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
                    "shm-size": 10000,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
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

TEST(ConfigParserDeathTest, KnownInstanceSpecifierWillNotDie)
{
    // Given a JSON with known instance specifier
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
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
                    "shm-size": 10000,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
                        }
                    ]
                }
            ]
        }
    ]
}
)"_json;

    // When parsing the JSON
    // That the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST(ConfigParserDeathTest, InvalidServiceInstanceSpecifierWillDie)
{
    // configuration is the same as the test above and is testing the positive case.
    // Given a JSON with invalid instance specifier
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
                        }
                    ]
                }
            ]
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "invalid_instance_specifier/",
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
                    "shm-size": 10000,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
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

TEST_F(ConfigParserFixtureDeathTest, NoServiceTypeFieldsOrEventsWillDie)
{
    // Given a JSON with no service type fields or events
    auto j2 = R"(
{
    "serviceTypes": [
        {
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "bindings": []
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
                    "shm-size": 10000,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
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

TEST_F(ConfigParserFixtureDeathTest, WithServiceTypeFieldsOrEventsWillNotDie)
{
    // configuration is the same as the test above and is testing the positive case.
    // Given a JSON with service type fields or events
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
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
                    "shm-size": 10000,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
                        }
                    ]
                }
            ]
        }
    ]
}

)"_json;
    // When parsing the JSON
    // That the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST_F(ConfigParserFixtureDeathTest, DuplicateServiceInstancesWillDie)
{
    // Given a JSON with duplicate instances
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
                        }
                    ]
                }
            ]
        },
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                  "asil-level": "QM",
                  "binding": "SHM"
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

TEST_F(ConfigParserFixtureDeathTest, NoDuplicateServiceInstancesWillNotDie)
{
    // configuration is the same as the test above and is testing the positive case.
    // Given a JSON without duplicate instances
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
                        }
                    ]
                }
            ]
        }
    ]
}
)"_json;
    // When parsing the JSON
    // That the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

TEST_F(ConfigParserFixtureDeathTest, MissingServiceTypeVersionWillDie)
{
    // Given a JSON with duplicate instances
    auto j2 = R"(
{
    "serviceTypes": [
        {
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "bindings": [
                {
                    "binding": "SHM",
                    "serviceId": 1234,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 30
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
                   "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
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

TEST_F(ConfigParserFixtureDeathTest, MissingServiceTypeMajorVersionWillDie)
{
    // Given a JSON with duplicate instances
    auto j2 = R"(
{
    "serviceTypes": [
        {
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "minor": 34
            },
            "bindings": [
                {
                    "binding": "SHM",
                    "serviceId": 1234,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 30
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
                   "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
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

TEST_F(ConfigParserFixtureDeathTest, MissingServiceTypeMinorVersionWillDie)
{
    // Given a JSON with duplicate instances
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 30
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
                "major": 12
            },
            "instances": [
                {
                    "instanceId": 1234,
                    "asil-level": "QM",
                    "binding": "SHM",
                   "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
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

TEST(ConfigParserTest, ValidServiceTypeVersionWillNotDie)
{
    // configuration is the same as the two tests above and is testing the positive case.
    // Given a JSON without duplicate event
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
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 30
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
                   "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft"
                        }
                    ]
                }
            ]
        }
    ]
}

)"_json;

    // When parsing the JSON
    // That the application will not terminate
    score::mw::com::impl::configuration::Parse(std::move(j2));
}

}  // namespace
}  // namespace score::mw::com::impl
