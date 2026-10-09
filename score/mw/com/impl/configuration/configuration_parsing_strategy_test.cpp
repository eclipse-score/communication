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
#include "score/mw/com/impl/configuration/configuration_flatbuffer_parsing_strategy.h"
#include "score/mw/com/impl/configuration/configuration_json_parsing_strategy.h"

#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/quality/compiler_warnings/warnings.h"

#include <score/assert_support.hpp>

#include "gmock/gmock.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <fstream>
#include <iostream>

// Every configuration used by these tests is a file in test/parsing_strategy_configs. The tests of
// ConfigurationParsingStrategyTest (and the other suites parametrised with kStrategies) run once per parsing strategy:
// ConfigurationJsonParsingStrategy parses <config>.json and ConfigurationFlatbufferParsingStrategy parses <config>.bin,
// which is converted from the very same <config>.json at build time. Configurations which flatc refuses to convert
// (see FLATC_REJECTED_CONFIGS in test/parsing_strategy_configs/BUILD) are only tested with the JSON strategy, in
// ConfigurationJsonOnlyParsingTest.
namespace score::mw::com::impl
{

namespace
{

using std::string_view_literals::operator""sv;

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

const std::string kTracingTraceFilterConfigPathDefaultValue{"./etc/mw_com_trace_filter.json"};

enum class Strategy : std::uint8_t
{
    kJson,
    kFlatbuffer,
};

// ConfigurationFlatbufferParsingStrategy is only implemented when built with --config=flatbuffers.
const std::vector<Strategy> kStrategies{
    Strategy::kJson,
#if defined(SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED)
    Strategy::kFlatbuffer,
#endif
};

std::string StrategyName(const ::testing::TestParamInfo<Strategy>& info)
{
    return info.param == Strategy::kFlatbuffer ? "Flatbuffer" : "Json";
}

// Tests may run either from the workspace root or from within an external repository's runfiles tree.
std::string GetPath(const std::string& relative_path)
{
    const std::string default_path = "score/mw/com/impl/configuration/" + relative_path;

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

Configuration ParseFile(const Strategy strategy, const std::string& relative_path_without_extension)
{
    if (strategy == Strategy::kFlatbuffer)
    {
        return configuration::ConfigurationFlatbufferParsingStrategy{}.Parse(
            GetPath(relative_path_without_extension + ".bin"));
    }
    return configuration::ConfigurationJsonParsingStrategy{}.Parse(GetPath(relative_path_without_extension + ".json"));
}

/// \brief Parses the configuration test/parsing_strategy_configs/<config_name> with the given strategy.
Configuration Parse(const Strategy strategy, const std::string& config_name)
{
    return ParseFile(strategy, "test/parsing_strategy_configs/" + config_name);
}

/// \brief Parses a configuration which only exists as JSON, since flatc rejects it.
Configuration ParseJson(const std::string& config_name)
{
    return Parse(Strategy::kJson, config_name);
}

class ConfigurationParsingStrategyTest : public ::testing::TestWithParam<Strategy>
{
  public:
    ServiceIdentifierType si_{make_ServiceIdentifierType("/score/ncar/services/TirePressureService", 12U, 34U)};
    ServiceVersionType sv_{make_ServiceVersionType(12U, 34U)};
    std::pair<const ServiceIdentifierType*, const ServiceVersionType*> found_service_type_{&si_, &sv_};
};

using ConfigurationParsingStrategyDeathTest = ConfigurationParsingStrategyTest;
using ConfigurationParsingStrategyTracingTest = ConfigurationParsingStrategyTest;
using TracingFilterConfigGetNumberOfTraceingSlots = ConfigurationParsingStrategyTest;

INSTANTIATE_TEST_SUITE_P(AllStrategies,
                         ConfigurationParsingStrategyTest,
                         ::testing::ValuesIn(kStrategies),
                         StrategyName);
INSTANTIATE_TEST_SUITE_P(AllStrategies,
                         ConfigurationParsingStrategyDeathTest,
                         ::testing::ValuesIn(kStrategies),
                         StrategyName);
INSTANTIATE_TEST_SUITE_P(AllStrategies,
                         ConfigurationParsingStrategyTracingTest,
                         ::testing::ValuesIn(kStrategies),
                         StrategyName);
INSTANTIATE_TEST_SUITE_P(AllStrategies,
                         TracingFilterConfigGetNumberOfTraceingSlots,
                         ::testing::ValuesIn(kStrategies),
                         StrategyName);

class ConfigurationJsonOnlyParsingTest : public ::testing::Test
{
};

TEST_P(ConfigurationParsingStrategyTest, ParseExampleJson)
{
    RecordProperty("Verifies", "SCR-21803701, SCR-21803702, SCR-5898925, SCR-5899090, SCR-5899184, SCR-7088394");
    RecordProperty("Description", "Checks whether all necessary configuration can be read at runtime");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // example/mw_com_config.json resp. its FlatBuffer conversion converter/mw_com_config.bin
    const auto config = GetParam() == Strategy::kFlatbuffer
                            ? ParseFile(Strategy::kFlatbuffer, "converter/mw_com_config")
                            : ParseFile(Strategy::kJson, "example/mw_com_config");

    const auto& deployments =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    EXPECT_EQ(deployments.service_, si_);
    EXPECT_EQ(ServiceIdentifierTypeView{deployments.service_}.GetVersion(), make_ServiceVersionType(12U, 34U));

    const auto secondDeploymentInfo = std::get<LolaServiceInstanceDeployment>(deployments.bindingInfo_);
    EXPECT_EQ(deployments.asilLevel_, QualityType::kASIL_B);
    EXPECT_EQ(secondDeploymentInfo.instance_id_.value(), LolaServiceInstanceId{1234U});
    EXPECT_EQ(*secondDeploymentInfo.shared_memory_size_, 10000);
    EXPECT_EQ(*secondDeploymentInfo.control_asil_b_memory_size_, 20000);
    EXPECT_EQ(*secondDeploymentInfo.control_qm_memory_size_, 30000);

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
    EXPECT_EQ(secondDeploymentInfo.events_.at("CurrentPressureFrontLeft").enforce_max_samples_, true);
    EXPECT_EQ(secondDeploymentInfo.events_.at("CurrentPressureFrontLeft").max_concurrent_allocations_.value(), 1);

    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft")
                  .lola_event_instance_deployment_.GetNumberOfTracingSlots(),
              7);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft")
                  .lola_event_instance_deployment_.GetNumberOfSampleSlots()
                  .value(),
              60 + 7);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft")
                  .lola_event_instance_deployment_.max_subscribers_.value(),
              6);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft")
                  .lola_event_instance_deployment_.enforce_max_samples_,
              true);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft")
                  .lola_event_instance_deployment_.max_concurrent_allocations_.value(),
              1);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_get_if_available_, true);
    EXPECT_EQ(secondDeploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_set_if_available_, true);
    EXPECT_TRUE(secondDeploymentInfo.methods_.at("SetPressure").enabled_);
    EXPECT_EQ(secondDeploymentInfo.methods_.at("SetPressure").queue_size_.value(), 20);

    const auto& service_deployment = config.GetServiceTypeDeployment(deployments.service_).value().get();
    const auto* const lola_service_type_deployment =
        std::get_if<LolaServiceTypeDeployment>(&service_deployment.binding_info_);
    ASSERT_NE(lola_service_type_deployment, nullptr);
    EXPECT_EQ(lola_service_type_deployment->service_id_, 1234);
    EXPECT_EQ(lola_service_type_deployment->events_.at("CurrentPressureFrontLeft"), 20);
    EXPECT_EQ(lola_service_type_deployment->fields_.at("CurrentTemperatureFrontLeft"), 30);
    EXPECT_EQ(lola_service_type_deployment->methods_.at("SetPressure"), 40);

    const auto& tracing_config = config.GetTracingConfiguration();
    EXPECT_TRUE(tracing_config.IsTracingEnabled());
    EXPECT_EQ(tracing_config.GetApplicationInstanceID(), "ara_com_example");
    EXPECT_EQ(tracing_config.GetTracingFilterConfigPath(), "./mw_com_trace_filter.json");
    // CurrentTemperatureFrontLeft configures numberOfIpcTracingSlots, so tracing is enabled for it
    const tracing::ServiceElementIdentifierView field_view{
        "/score/ncar/services/TirePressureService", "CurrentTemperatureFrontLeft", ServiceElementType::FIELD};
    EXPECT_TRUE(tracing_config.IsServiceElementTracingEnabled(field_view, "abc/abc/TirePressurePort"));

    EXPECT_EQ(config.GetGlobalConfiguration().GetProcessAsilLevel(), QualityType::kASIL_B);
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_QM), 8);
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_B), 5);
    EXPECT_EQ(config.GetGlobalConfiguration().GetSenderMessageQueueSize(), 12);
    EXPECT_EQ(config.GetGlobalConfiguration().GetApplicationId(), 1234);

    EXPECT_EQ(config.GetGlobalConfiguration().GetShmSizeCalcMode(), ShmSizeCalculationMode::kSimulation);
}

TEST_P(ConfigurationParsingStrategyTest, InvalidPathWillDie)
{
    // Given an invalid path that doesn't point to a configuration file

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ParseFile(GetParam(), "my_invalid_path_to_nowhere"));
}

TEST_P(ConfigurationParsingStrategyTest, NoServiceInstanceWillDie)
{
    // Given a JSON without necessary attribute `serviceInstances`

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_service_instances"));
}

TEST_P(ConfigurationParsingStrategyTest, NoServiceNameInInstanceWillDie)
{
    // Given a JSON without necessary attribute `serviceName`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_instance_specifier_in_empty_instance"));
}

TEST_P(ConfigurationParsingStrategyTest, NoServiceTypesWillDie)
{
    // Given a JSON without necessary attribute `serviceTypes`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_service_types"));
}

TEST_P(ConfigurationParsingStrategyTest, ParseSomeIpBinding)
{
    // Given a JSON which configures a service type and a service instance with a SOME/IP binding

    // When parsing the JSON
    const auto config = Parse(GetParam(), "someip_binding");

    // Then the service type deployment holds a SOME/IP binding with the configured ids
    const auto& service_type_deployment =
        config
            .GetServiceTypeDeployment(make_ServiceIdentifierType("/score/ncar/services/TirePressureService", 12U, 34U))
            .value()
            .get();
    const auto* const someip_service_type_deployment =
        std::get_if<SomeIpServiceTypeDeployment>(&service_type_deployment.binding_info_);
    ASSERT_NE(someip_service_type_deployment, nullptr);
    EXPECT_EQ(someip_service_type_deployment->service_id_, 1234U);
    ASSERT_EQ(someip_service_type_deployment->events_.count("CurrentPressureFrontLeft"), 1U);
    EXPECT_EQ(someip_service_type_deployment->events_.at("CurrentPressureFrontLeft"), 20U);
    ASSERT_EQ(someip_service_type_deployment->methods_.count("SetPressure"), 1U);
    EXPECT_EQ(someip_service_type_deployment->methods_.at("SetPressure"), 40U);

    // And the service instance deployment holds a SOME/IP binding with the configured instance id and event
    const auto& service_instance_deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();
    EXPECT_EQ(service_instance_deployment.GetBindingType(), BindingType::kSomeIp);

    const auto* const someip_service_instance_deployment =
        std::get_if<SomeIpServiceInstanceDeployment>(&service_instance_deployment.bindingInfo_);
    ASSERT_NE(someip_service_instance_deployment, nullptr);
    ASSERT_TRUE(someip_service_instance_deployment->instance_id_.has_value());
    EXPECT_EQ(someip_service_instance_deployment->instance_id_.value().GetId(), 1234U);
    ASSERT_TRUE(someip_service_instance_deployment->ContainsEvent("CurrentPressureFrontLeft"));

    const auto& event_instance_deployment = someip_service_instance_deployment->events_.at("CurrentPressureFrontLeft");
    EXPECT_EQ(event_instance_deployment.GetNumberOfSampleSlots().value(), 50U);
    EXPECT_EQ(event_instance_deployment.max_subscribers_.value(), 5U);
}

TEST_P(ConfigurationParsingStrategyTest, NoServiceNameForServiceType)
{
    // Given a JSON without necessary attribute `serviceTypeName`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_service_type_name"));
}

TEST_P(ConfigurationParsingStrategyTest, NoVersionForServiceTypeDeployment)
{
    // Given a JSON without necessary attribute `version`

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_service_type_version"));
}

TEST_P(ConfigurationParsingStrategyTest, NoBindingsForServiceTypeDeployment)
{
    // Given a JSON without necessary attribute `bindings`

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_service_type_bindings"));
}

TEST_P(ConfigurationParsingStrategyTest, NoBindingIdentifierInServiceTypeDeployment)
{
    // Given a JSON without necessary attribute `binding`

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_service_type_binding_identifier"));
}

TEST_P(ConfigurationParsingStrategyTest, NoServiceIdInServiceTypeDeployment)
{
    // Given a JSON without necessary attribute `serviceId`

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_service_type_service_id"));
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, UnknownBindingIdentifierInServiceTypeDeployment)
{
    // Given a JSON with an unknown binding identifier

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ParseJson("unknown_service_type_binding"));
}

TEST_P(ConfigurationParsingStrategyTest, NoEventNameWillCauseTermination)
{
    // Given a JSON with a missing event name

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_type_event_name"));
}

TEST_P(ConfigurationParsingStrategyTest, NoFieldNameWillCauseTermination)
{
    // Given a JSON with a missing field name

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_type_field_name"));
}

TEST_P(ConfigurationParsingStrategyTest, NoEventIdWillCauseTermination)
{
    // Given a JSON with a missing event id

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_type_event_id"));
}

TEST_P(ConfigurationParsingStrategyTest, NoFieldIdWillCauseTermination)
{
    // Given a JSON with a missing field id

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_type_field_id"));
}

TEST_P(ConfigurationParsingStrategyTest, NoMethodNameWillCauseTermination)
{
    // Given a JSON with a missing method name

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_type_method_name"));
}

TEST_P(ConfigurationParsingStrategyTest, NoMethodIdWillCauseTermination)
{
    // Given a JSON with a missing method id

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_type_method_id"));
}

TEST_P(ConfigurationParsingStrategyTest, DuplicateTypeMethodNameWillDie)
{
    // Given a JSON with a duplicate LoLa method type deployment (duplicate methodName)

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_type_method_name"));
}

TEST_P(ConfigurationParsingStrategyTest, MethodIdMatchingEventIdIsNotAllowed)
{
    // Given a JSON with a method id which is the same as an event id of the same binding

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "method_id_matches_event_id"));
}

TEST_P(ConfigurationParsingStrategyTest, LolaMethodWithoutNameCausesTermination)
{
    // Given a JSON with a LoLa method instance deployment without a methodName

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "instance_method_without_name"));
}

TEST_P(ConfigurationParsingStrategyTest, DuplicateInstanceMethodNameWillDie)
{
    // Given a JSON with a duplicate LoLa method instance deployment (duplicate methodName)

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_instance_method_name"));
}

TEST_P(ConfigurationParsingStrategyTest, MethodAttributesAreParsed)
{
    // Given a JSON with a method which sets all of its attributes
    // When parsing the JSON
    const auto config = Parse(GetParam(), "method_all_attributes");

    // Then the service type deployment contains the method id
    const auto& service_type_deployment =
        config
            .GetServiceTypeDeployment(make_ServiceIdentifierType("/score/ncar/services/TirePressureService", 12U, 34U))
            .value()
            .get();
    const auto& lola_service_type_deployment =
        std::get<LolaServiceTypeDeployment>(service_type_deployment.binding_info_);
    ASSERT_EQ(lola_service_type_deployment.methods_.count("SetPressure"), 1U);
    EXPECT_EQ(lola_service_type_deployment.methods_.at("SetPressure"), 40U);

    // And the service instance deployment contains the configured queue size and the disabled flag
    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();
    const auto& lola_deployment = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    ASSERT_TRUE(lola_deployment.ContainsMethod("SetPressure"));
    const auto& method_deployment = lola_deployment.methods_.at("SetPressure");
    ASSERT_TRUE(method_deployment.queue_size_.has_value());
    EXPECT_EQ(method_deployment.queue_size_.value(), 255U);
    EXPECT_FALSE(method_deployment.enabled_);
}

TEST_P(ConfigurationParsingStrategyTest, MethodOmittedOptionalsUseDefaults)
{
    // Given a JSON with a method instance deployment which only sets the methodName
    // When parsing the JSON
    const auto config = Parse(GetParam(), "method_defaults");

    // Then the queue size is not set and the method is enabled
    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();
    const auto& lola_deployment = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    ASSERT_TRUE(lola_deployment.ContainsMethod("SetPressure"));
    const auto& method_deployment = lola_deployment.methods_.at("SetPressure");
    EXPECT_FALSE(method_deployment.queue_size_.has_value());
    EXPECT_TRUE(method_deployment.enabled_);
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, WrongPermissionValueWillCauseTermination)
{
    // Given a JSON with an invalid permission in permission-check attribute

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ParseJson("wrong_permission_checks_value"));
}

TEST_P(ConfigurationParsingStrategyTest, DuplicateEventTypeDeploymentWillCauseTermination)
{
    // Given a JSON with an duplicate LoLa event type deployment (duplicate eventName)

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_type_event_name"));
}

TEST_P(ConfigurationParsingStrategyTest, DuplicateFieldTypeDeploymentWillCauseTermination)
{
    // Given a JSON with an duplicate LoLa field type deployment (duplicate fieldName)

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_type_field_name"));
}

TEST_P(ConfigurationParsingStrategyTest, DuplicateServiceTypeDeploymentWillCauseTermination)
{
    // Given a JSON with a duplicate service type deployment (duplicate serviceTypeName/version)

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_service_type"));
}

TEST_P(ConfigurationParsingStrategyTest, NoInstanceSpecifierInInstanceWillDie)
{
    // Given a JSON without necessary attribute `instanceSpecifier`

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_instance_specifier"));
}

TEST_P(ConfigurationParsingStrategyTest, NoVersionInInstanceWillDie)
{
    // Given a JSON without necessary attribute `version`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_instance_version"));
}

TEST_P(ConfigurationParsingStrategyTest, NoVersionDetailsInInstanceWillDie)
{
    // Given a JSON without necessary attribute `major`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_instance_major_version"));
}

TEST_P(ConfigurationParsingStrategyTest, NoDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON without necessary attribute `instances`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_deployment_instances"));
}

TEST_P(ConfigurationParsingStrategyTest, EmptyDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON without elements in array `instances`.
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "empty_deployment_instances"));
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, UnknownDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON with an unknown binding "HappyHippo" in an instance deployment.
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ParseJson("unknown_instance_binding"));
}

TEST_P(ConfigurationParsingStrategyTest, NoAsilInDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON without necessary attribute `asil-level`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_instance_asil_level"));
}

TEST_P(ConfigurationParsingStrategyTest, NoBindingInfoInDeploymentInstancesInInstanceWillDie)
{
    // Given a JSON without necessary attribute `binding`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "no_instance_binding"));
}

TEST_P(ConfigurationParsingStrategyTest, LolaEventWithoutNameCausesTermination)
{
    // Given a JSON without necessary attribute `name` for an event for Shm-Binding Info
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "instance_event_without_name"));
}

TEST_P(ConfigurationParsingStrategyTest, LolaFieldWithoutNameCausesTermination)
{
    // Given a JSON without necessary attribute `name` for a field for Shm-Binding Info
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "instance_field_without_name"));
}

TEST_P(ConfigurationParsingStrategyTest, LolaEventIdDuplicateCausesTermination)
{
    // Given a JSON where a LoLa event id has been duplicated
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_type_event_id"));
}

TEST_P(ConfigurationParsingStrategyTest, LolaFieldIdDuplicateCausesTermination)
{
    // Given a JSON where a LoLa event id has been duplicated
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_type_field_id"));
}

TEST_P(ConfigurationParsingStrategyTest, LolaMatchingEventAndFieldIdsIsNotAllowed)
{
    // Given a JSON where a LoLa field has been duplicated
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "matching_event_and_field_ids"));
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, LolaIncorrectEventNameCausesTermination)
{
    // Given a JSON where a LoLa event name is incorrect, not 'EventName'
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ParseJson("instance_event_unknown_name_key"));
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, LolaIncorrectFieldNameCausesTermination)
{
    // Given a JSON where a LoLa field name is incorrect, not 'fieldName'
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ParseJson("instance_field_unknown_name_key"));
}

TEST_P(ConfigurationParsingStrategyTest, LolaEventMaxSamplesAndNumberOfSampleSlotsCausesTermination)
{
    // Given a JSON where a LoLa event has both properties configured maxSamples (deprecated) and numberOfSampleSlots
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        Parse(GetParam(), "event_max_samples_and_number_of_sample_slots"));
}

TEST_P(ConfigurationParsingStrategyTest, NoEventMaxSubscribersLeavesValueOptional)
{
    // Given a JSON where a LoLa event has no configured max-subscribers
    // When parsing the JSON
    const auto config = Parse(GetParam(), "no_event_max_subscribers");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    // That the max_subscribers_ in the event has no value
    EXPECT_FALSE(deploymentInfo.events_.at("CurrentPressureFrontLeft").max_subscribers_.has_value());
}

TEST_P(ConfigurationParsingStrategyTest, NoFieldMaxSubscribersLeavesValueOptional)
{
    // Given a JSON where a LoLa field has no configured max-subscribers
    // When parsing the JSON

    const auto config = Parse(GetParam(), "no_field_max_subscribers");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    // That the max_subscribers_ in the field has no value
    EXPECT_FALSE(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft")
                     .lola_event_instance_deployment_.max_subscribers_.has_value());
}

TEST_P(ConfigurationParsingStrategyTest, NoSHMInstanceIdLeavesValueOptional)
{
    // Given a JSON without necessary attribute `instance_id_` for SHM-Binding Info
    const auto config = Parse(GetParam(), "no_shm_instance_id");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    ASSERT_FALSE(deploymentInfo.instance_id_.has_value());
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, LolaEventOptionalMaxConcurrentAllocations)
{
    // Given a JSON with an event with optional max concurrent allocations set

    // When parsing such a configuration
    // Fail and abort
    EXPECT_EXIT(ParseJson("event_max_concurrent_allocations"), ::testing::KilledBySignal(SIGABRT), ".*");
}

TEST_P(ConfigurationParsingStrategyTest, LolaEventDeprecatedMaxSamplesGetsRecognized)
{
    // Given a JSON with an event with deprecated maxSamples property is still recognized
    const auto config = Parse(GetParam(), "event_deprecated_max_samples");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    EXPECT_EQ(deploymentInfo.events_.at("CurrentPressureFrontLeft").GetNumberOfSampleSlots().value(), 50);
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, LolaFieldOptionalMaxConcurrentAllocations)
{
    // Given a JSON with a field with optional max concurrent allocations set
    // When parsing such a configuration
    // Fail and abort
    EXPECT_EXIT(ParseJson("field_max_concurrent_allocations"), ::testing::KilledBySignal(SIGABRT), ".*");
}

TEST_P(ConfigurationParsingStrategyTest, LolaEventOptionalEnforceMaxSamples)
{
    RecordProperty("Verifies", "SCR-7088394");
    RecordProperty("Description", "Checks whether optional 'enforceMaxSamples' configuration can be read at runtime");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with optional attribute `enforceMaxSamples` for SHM-Binding Info
    const auto config = Parse(GetParam(), "event_enforce_max_samples_false");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    EXPECT_EQ(deploymentInfo.events_.at("CurrentPressureFrontLeft").enforce_max_samples_, false);
}

TEST_P(ConfigurationParsingStrategyTest, LolaFieldOptionalEnforceMaxSamples)
{
    // Given a JSON with optional attribute `enforceMaxSamples` for SHM-Binding Info
    const auto config = Parse(GetParam(), "field_enforce_max_samples_false");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    EXPECT_EQ(
        deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").lola_event_instance_deployment_.enforce_max_samples_,
        false);
}

TEST_P(ConfigurationParsingStrategyTest, LolaFieldUseGetIfAvailableSetToFalse)
{
    // Given a JSON with optional attribute `useGetIfAvailable` set to false for a field

    // When parsing the JSON
    const auto config = Parse(GetParam(), "field_use_get_if_available_false");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);

    // Then use_get_if_available_ is false and use_set_if_available_ defaults to true
    EXPECT_EQ(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_get_if_available_, false);
    EXPECT_EQ(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_set_if_available_, true);
}

TEST_P(ConfigurationParsingStrategyTest, LolaFieldUseSetIfAvailableSetToFalse)
{
    // Given a JSON with optional attribute `useSetIfAvailable` set to false for a field

    // When parsing the JSON
    const auto config = Parse(GetParam(), "field_use_set_if_available_false");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);

    // Then use_set_if_available_ is false and use_get_if_available_ defaults to true
    EXPECT_EQ(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_get_if_available_, true);
    EXPECT_EQ(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_set_if_available_, false);
}

TEST_P(ConfigurationParsingStrategyTest, LolaFieldOmittingBothFlagsDefaultsToBothTrue)
{
    // Given a JSON for a field without `useGetIfAvailable` or `useSetIfAvailable`

    // When parsing the JSON
    const auto config = Parse(GetParam(), "field_use_flags_omitted");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);

    // Then both flags default to true
    EXPECT_EQ(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_get_if_available_, true);
    EXPECT_EQ(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_set_if_available_, true);
}

TEST_P(ConfigurationParsingStrategyTest, LolaFieldBothFlagsExplicitlySetToFalse)
{
    // Given a JSON with both `useGetIfAvailable` and `useSetIfAvailable` explicitly set to false for a field

    // When parsing the JSON
    const auto config = Parse(GetParam(), "field_use_flags_both_false");

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();

    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);

    // Then both flags are false (explicit false overrides the default of true)
    EXPECT_EQ(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_get_if_available_, false);
    EXPECT_EQ(deploymentInfo.fields_.at("CurrentTemperatureFrontLeft").use_set_if_available_, false);
}

TEST_P(ConfigurationParsingStrategyTest, EmptyServiceTypes)
{
    // Given a JSON with necessary attribute `serviceTypes` being empty (which is allowed)
    // When parsing the JSON
    // That the application will terminate
    Configuration config{Parse(GetParam(), "empty_service_types_and_instances")};
    EXPECT_EQ(config.GetNumberOfServiceTypes(), 0);
    EXPECT_EQ(config.GetNumberOfServiceInstances(), 0);
    EXPECT_TRUE(config.IsServiceInstancesEmpty());

    // And the omitted `global` and `tracing` sections fall back to their defaults
    const auto& global = config.GetGlobalConfiguration();
    EXPECT_EQ(global.GetProcessAsilLevel(), QualityType::kASIL_QM);
    EXPECT_FALSE(global.GetApplicationId().has_value());
    EXPECT_EQ(global.GetReceiverMessageQueueSize(QualityType::kASIL_QM),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE);
    EXPECT_EQ(global.GetReceiverMessageQueueSize(QualityType::kASIL_B),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE);
    EXPECT_EQ(global.GetSenderMessageQueueSize(), GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_TX_QUEUE);
    EXPECT_EQ(global.GetShmSizeCalcMode(), ShmSizeCalculationMode::kSimulation);
    EXPECT_FALSE(config.GetTracingConfiguration().IsTracingEnabled());
}

TEST_P(ConfigurationParsingStrategyTest, EmptyServiceElementListsParse)
{
    // Given a JSON whose service type binding and service instance have empty events, fields and methods lists
    // When parsing the JSON
    const auto config = Parse(GetParam(), "empty_service_elements");

    // Then neither the service type deployment nor the service instance deployment contain any service element
    const auto& service_type_deployment =
        config
            .GetServiceTypeDeployment(make_ServiceIdentifierType("/score/ncar/services/TirePressureService", 12U, 34U))
            .value()
            .get();
    const auto& lola_service_type_deployment =
        std::get<LolaServiceTypeDeployment>(service_type_deployment.binding_info_);
    EXPECT_TRUE(lola_service_type_deployment.events_.empty());
    EXPECT_TRUE(lola_service_type_deployment.fields_.empty());
    EXPECT_TRUE(lola_service_type_deployment.methods_.empty());

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();
    const auto& lola_deployment = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    EXPECT_TRUE(lola_deployment.events_.empty());
    EXPECT_TRUE(lola_deployment.fields_.empty());
    EXPECT_TRUE(lola_deployment.methods_.empty());
}

TEST_P(ConfigurationParsingStrategyTest, OmittedServiceElementListsParse)
{
    // Given a JSON whose service type binding and service instance omit the events, fields and methods keys
    // When parsing the JSON
    const auto config = Parse(GetParam(), "no_service_element_lists");

    // Then neither the service type deployment nor the service instance deployment contain any service element
    const auto& service_type_deployment =
        config
            .GetServiceTypeDeployment(make_ServiceIdentifierType("/score/ncar/services/TirePressureService", 12U, 34U))
            .value()
            .get();
    const auto& lola_service_type_deployment =
        std::get<LolaServiceTypeDeployment>(service_type_deployment.binding_info_);
    EXPECT_TRUE(lola_service_type_deployment.events_.empty());
    EXPECT_TRUE(lola_service_type_deployment.fields_.empty());
    EXPECT_TRUE(lola_service_type_deployment.methods_.empty());

    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();
    const auto& lola_deployment = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    EXPECT_TRUE(lola_deployment.events_.empty());
    EXPECT_TRUE(lola_deployment.fields_.empty());
    EXPECT_TRUE(lola_deployment.methods_.empty());
}

TEST_P(ConfigurationParsingStrategyTest, StrictPermissionIsSet)
{
    // Given a JSON with `permission-checks` attribute which is set to `strict`
    // When parsing the JSON
    const auto configuration = Parse(GetParam(), "permission_checks_strict");
    ASSERT_FALSE(configuration.IsServiceInstancesEmpty());

    // That LolaServiceInstanceDeployment instance is obtained
    const auto& deployment =
        configuration.GetServiceInstanceDeployment(InstanceSpecifier::Create({"abc/abc/TirePressurePort"}).value())
            .value()
            .get();
    const auto* const lola_service_instance = std::get_if<LolaServiceInstanceDeployment>(&deployment.bindingInfo_);
    ASSERT_NE(lola_service_instance, nullptr);
    // And "permission-checks" attribute is set to "strict"
    EXPECT_TRUE(lola_service_instance->strict_permissions_);
}

TEST_P(ConfigurationParsingStrategyTest, GetNoneStrictIfNoPermissionFlagAttr)
{
    // Given a JSON without `permission-checks` attribute
    // When parsing the JSON
    const auto configuration = Parse(GetParam(), "no_event_max_subscribers");
    ASSERT_FALSE(configuration.IsServiceInstancesEmpty());

    // That LolaServiceInstanceDeployment instance is obtained
    const auto& deployment =
        configuration.GetServiceInstanceDeployment(InstanceSpecifier::Create({"abc/abc/TirePressurePort"}).value())
            .value()
            .get();
    const auto* const lola_service_instance = std::get_if<LolaServiceInstanceDeployment>(&deployment.bindingInfo_);
    ASSERT_NE(lola_service_instance, nullptr);
    // And "permission-checks" attribute is set to none-"strict"
    EXPECT_FALSE(lola_service_instance->strict_permissions_);
}

class ProcessAsil : public ::testing::TestWithParam<std::tuple<Strategy, std::tuple<std::string, QualityType>>>
{
};

TEST_P(ProcessAsil, ValidProcessAsilLevel)
{
    const auto& [strategy, test_case] = GetParam();
    const auto& [config_name, expected_asil_level] = test_case;
    Configuration config{Parse(strategy, config_name)};
    EXPECT_EQ(config.GetGlobalConfiguration().GetProcessAsilLevel(), expected_asil_level);
}

const std::vector<std::tuple<std::string, QualityType>> valid_global_asil{
    {"process_asil_qm", QualityType::kASIL_QM},
    {"process_asil_b", QualityType::kASIL_B},
    {"empty_service_types_and_instances", QualityType::kASIL_QM},
};

INSTANTIATE_TEST_SUITE_P(ValidProcessAsil,
                         ProcessAsil,
                         ::testing::Combine(::testing::ValuesIn(kStrategies), ::testing::ValuesIn(valid_global_asil)));

class InvalidProcessAsil : public ::testing::TestWithParam<std::string>
{
};

TEST_P(InvalidProcessAsil, DieOnInvalidAsil)
{
    DISABLE_WARNING_PUSH
    DISABLE_WARNING_UNUSED_VALUE  // Comming from gtest, try removing when gtest 1.12 or higher

        SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Configuration{ParseJson(GetParam())});

    DISABLE_WARNING_POP
}

// JSON only: flatc rejects asil-levels which are no AsilLevel enum symbol.
INSTANTIATE_TEST_SUITE_P(InvalidProcessAsil,
                         InvalidProcessAsil,
                         ::testing::Values("process_asil_any", "process_asil_elefant", "process_asil_empty_string"));

class InvalidMsgQueueSizeFixture : public ::testing::TestWithParam<std::string>
{
};

TEST_P(InvalidMsgQueueSizeFixture, DieOnInvalidMessageQueueSize)
{
    DISABLE_WARNING_PUSH
    DISABLE_WARNING_UNUSED_VALUE  // Comming from gtest, try removing when gtest 1.12 or higher

        SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Configuration{ParseJson(GetParam())});

    DISABLE_WARNING_POP
}

// JSON only: flatc rejects queue sizes which are no numbers.
INSTANTIATE_TEST_SUITE_P(InvalidMsgQueueSizeTests,
                         InvalidMsgQueueSizeFixture,
                         ::testing::Values("queue_size_b_receiver_not_a_number",
                                           "queue_size_b_receiver_not_a_number_with_b_sender",
                                           "queue_size_b_sender_not_a_number",
                                           "queue_size_qm_receiver_not_a_number"));

class InvalidApplicationIdFixture : public ::testing::TestWithParam<std::tuple<Strategy, std::string>>
{
};

TEST_P(InvalidApplicationIdFixture, DieOnInvalidApplicationId)
{
    const auto& [strategy, config_name] = GetParam();
    // Given a configuration with invalid applicationID

    // When parsing the configuration then it will fail with a precondition violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = Configuration{Parse(strategy, config_name)});
}

INSTANTIATE_TEST_SUITE_P(InvalidApplicationIdTests,
                         InvalidApplicationIdFixture,
                         ::testing::Combine(::testing::ValuesIn(kStrategies),
                                            ::testing::Values(std::string{"application_id_uint32_max"})));

// JSON only: flatc rejects applicationIDs which do not fit into an uint32.
INSTANTIATE_TEST_SUITE_P(InvalidApplicationIdJsonOnlyTests,
                         InvalidApplicationIdFixture,
                         ::testing::Combine(::testing::Values(Strategy::kJson),
                                            ::testing::Values(std::string{"application_id_too_large"},
                                                              std::string{"application_id_negative"})));

TEST_P(ConfigurationParsingStrategyTest, OnlyQmReceiverQueueSizes)
{
    // Given a JSON with only QM-receiver queue size being explicitly configured
    // When parsing the JSON
    const auto config = Parse(GetParam(), "queue_size_only_qm_receiver");
    // expect that the QM-receiver has the configured value
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_QM), 8);
    // and that the not explicit configured B-receiver has the default value (DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE)
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_B),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE);
    // and that the not explicit configured B-sender has the default value (DEFAULT_MIN_NUM_MESSAGES_TX_QUEUE)
    EXPECT_EQ(config.GetGlobalConfiguration().GetSenderMessageQueueSize(),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_TX_QUEUE);
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, InvalidQualityTypeForAllowedConsumersWillDie)
{
    // Given a JSON without invalid attribute consumer quality type
    // When parsing the JSON
    // Then the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ParseJson("allowed_consumer_invalid_quality_type"));
}

class ShmSizeCalcMode
    : public ::testing::TestWithParam<std::tuple<Strategy, std::tuple<std::string, ShmSizeCalculationMode>>>
{
};

TEST_P(ShmSizeCalcMode, ValidShmSizeCalcMode)
{
    const auto& [strategy, test_case] = GetParam();
    const auto& [config_name, expected_shm_size_calc_mode] = test_case;
    Configuration config{Parse(strategy, config_name)};
    EXPECT_EQ(config.GetGlobalConfiguration().GetShmSizeCalcMode(), expected_shm_size_calc_mode);
}

const std::vector<std::tuple<std::string, ShmSizeCalculationMode>> valid_global_shm_size_calc_modes{
    {"shm_size_calc_mode_simulation", ShmSizeCalculationMode::kSimulation},
    {"shm_size_calc_mode_analysis", ShmSizeCalculationMode::kAnalysis},
    {"empty_service_types_and_instances", ShmSizeCalculationMode::kSimulation},
};

INSTANTIATE_TEST_SUITE_P(ValidShmSizeCalcMode,
                         ShmSizeCalcMode,
                         ::testing::Combine(::testing::ValuesIn(kStrategies),
                                            ::testing::ValuesIn(valid_global_shm_size_calc_modes)));

TEST_P(ConfigurationParsingStrategyTracingTest, EnablingGlobalTracingFlagSetsTracingEnabled)
{
    RecordProperty("Verifies", "SCR-18159733");
    RecordProperty("Description",
                   "TracingConfiguration IsTracingEnabled is true when global tracing enabled flag is set.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with the global tracing flag enabled
    // When parsing the JSON
    Configuration config{Parse(GetParam(), "tracing_enabled")};

    // Then tracing is enabled in the TracingConfiguration
    EXPECT_TRUE(config.GetTracingConfiguration().IsTracingEnabled());
}

TEST_P(ConfigurationParsingStrategyTracingTest, ProvidingAllTracingConfigElementsDoesNotCrash)
{
    RecordProperty("Verifies", "SCR-18143152, SCR-18159733");
    RecordProperty("Description",
                   "mw/com configuration file contains flag for enabling / disabling tracing (SCR-18143152). "
                   "TracingConfiguration IsTracingEnabled is false when global tracing enabled flag is not set "
                   "(SCR-18159733).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with all tracing attributes and the global tracing flag disabled
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{Parse(GetParam(), "tracing_disabled")};

    // Then tracing is disabled in the TracingConfiguration and all other attributes are parsed
    EXPECT_FALSE(config.GetTracingConfiguration().IsTracingEnabled());
    EXPECT_EQ(config.GetTracingConfiguration().GetApplicationInstanceID(), "test_application_id");
    EXPECT_EQ(config.GetTracingConfiguration().GetTracingFilterConfigPath(), "./test_filter_config.json");
}

TEST_P(ConfigurationParsingStrategyTracingTest, ProvidingAllRequiredTracingConfigElementsDoesNotCrash)
{
    RecordProperty("Verifies", "SCR-18143152, SCR-18143480");
    RecordProperty("Description",
                   "Flag for enabling / disabling tracing is optional (SCR-18143152). If the flag is not provided, it "
                   "will default to false (SCR-18143480).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with all tracing attributes
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{Parse(GetParam(), "tracing_only_application_instance_id")};

    EXPECT_FALSE(config.GetTracingConfiguration().IsTracingEnabled());
    EXPECT_EQ(config.GetTracingConfiguration().GetApplicationInstanceID(), "test_application_id");
}

TEST_P(ConfigurationParsingStrategyTracingTest,
       ParsingSucceedsIfApplicationInstanceIdentifierAndTraceFilterConfigPathPropertiesExistWhenTracingIsEnabled)
{
    RecordProperty("Verifies", "SCR-19177359, SCR-18144291");
    RecordProperty("Description",
                   "Checks that parsing succeeds if applicationInstanceID property is provided when tracing is enabled "
                   "(SCR-19177359) and if traceFilterConfigPath property is provided when the tracing section is "
                   "present in the configuration file (SCR-18144291).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with all tracing attributes
    // When parsing the JSON
    // That the application will not terminate
    score::cpp::ignore = Parse(GetParam(), "tracing_enabled_with_application_instance_id");
}

TEST_P(ConfigurationParsingStrategyTracingTest,
       ParsingTerminatesIfApplicationInstanceIdentifierPropertyDoesNotExistWhenTracingIsEnabled)
{
    RecordProperty("Verifies", "SCR-19177359");
    RecordProperty(
        "Description",
        "Checks that parsing terminates if applicationInstanceID property is not provided when tracing is enabled.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON which is missing applicationInstanceID
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        Parse(GetParam(), "tracing_enabled_without_application_instance_id"));
}

TEST_P(ConfigurationParsingStrategyTracingTest,
       ParsingSucceedsIfTraceFilterConfigPathPropertyDoesNotExistWhenTracingSectionIsPresent)
{
    RecordProperty("Verifies", "SCR-18144411");
    RecordProperty("Description",
                   "Checks that the traceFilterConfigPath property is optional and the default value is "
                   "./etc/mw_com_trace_filter.json.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a JSON with all tracing attributes except for traceFilterConfigPath
    // When parsing the JSON
    // That the application will not terminate
    const auto config{Parse(GetParam(), "tracing_without_filter_config_path")};

    EXPECT_EQ(config.GetTracingConfiguration().GetTracingFilterConfigPath(), "./etc/mw_com_trace_filter.json");
}

TEST_P(ConfigurationParsingStrategyTracingTest, ProvidingServiceElementEnabledEnablesServiceElementTracing)
{
    // Given a JSON with all tracing attributes
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{Parse(GetParam(), "tracing_service_elements_enabled")};
    const auto& tracing_config = config.GetTracingConfiguration();
    EXPECT_TRUE(tracing_config.IsTracingEnabled());

    tracing::ServiceElementIdentifierView service_1_event{
        "/score/ncar/services/TirePressureService", "CurrentPressureFrontLeft", ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_1_field{
        "/score/ncar/services/TirePressureService", "CurrentPressureFrontRight", ServiceElementType::FIELD};

    tracing::ServiceElementIdentifierView service_2_event{
        "/score/ncar/services/TireTemperatureService", "CurrentTemperatureFrontLeft", ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_2_field{
        "/score/ncar/services/TireTemperatureService", "CurrentTemperatureFrontRight", ServiceElementType::FIELD};

    const auto service_1_instance_specifier =
        InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
    const auto service_2_instance_specifier =
        InstanceSpecifier::Create(std::string{"abc/abc/TireTemperaturePort"}).value();
    const auto service_1_instance_specifier_string_view = service_1_instance_specifier.ToString();
    const auto service_2_instance_specifier_string_view = service_2_instance_specifier.ToString();

    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_event, service_1_instance_specifier_string_view));
    EXPECT_TRUE(
        tracing_config.IsServiceElementTracingEnabled(service_1_field, service_1_instance_specifier_string_view));
    EXPECT_TRUE(
        tracing_config.IsServiceElementTracingEnabled(service_2_event, service_2_instance_specifier_string_view));
    EXPECT_TRUE(
        tracing_config.IsServiceElementTracingEnabled(service_2_field, service_2_instance_specifier_string_view));
}

TEST_P(ConfigurationParsingStrategyTracingTest,
       DisablingGlobalTracingReturnsFalseForAllCallsToIsServiceElementTracingEnabled)
{
    // Given a JSON with all tracing attributes
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{Parse(GetParam(), "tracing_service_elements_global_disabled")};
    const auto& tracing_config = config.GetTracingConfiguration();
    EXPECT_FALSE(tracing_config.IsTracingEnabled());

    tracing::ServiceElementIdentifierView service_1_event{
        "/score/ncar/services/TirePressureService", "CurrentPressureFrontLeft", ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_1_field{
        "/score/ncar/services/TirePressureService", "CurrentPressureFrontRight", ServiceElementType::FIELD};

    tracing::ServiceElementIdentifierView service_2_event{
        "/score/ncar/services/TireTemperatureService", "CurrentTemperatureFrontLeft", ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_2_field{
        "/score/ncar/services/TireTemperatureService", "CurrentTemperatureFrontRight", ServiceElementType::FIELD};

    const auto service_1_instance_specifier =
        InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
    const auto service_2_instance_specifier =
        InstanceSpecifier::Create(std::string{"abc/abc/TireTemperaturePort"}).value();
    const auto service_1_instance_specifier_string_view = service_1_instance_specifier.ToString();
    const auto service_2_instance_specifier_string_view = service_2_instance_specifier.ToString();

    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_event, service_1_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_field, service_1_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_2_event, service_2_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_2_field, service_2_instance_specifier_string_view));
}

TEST_P(ConfigurationParsingStrategyTracingTest, NotProvidingServiceElementEnabledDisablesServiceElementTracing)
{
    // Given a JSON with all tracing attributes
    // When parsing the JSON
    // That the application will not terminate
    Configuration config{Parse(GetParam(), "tracing_service_elements_partially_enabled")};
    const auto& tracing_config = config.GetTracingConfiguration();
    EXPECT_TRUE(tracing_config.IsTracingEnabled());

    tracing::ServiceElementIdentifierView service_1_event{
        "/score/ncar/services/TirePressureService", "CurrentPressureFrontLeft", ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_1_field{
        "/score/ncar/services/TirePressureService", "CurrentPressureFrontRight", ServiceElementType::FIELD};

    tracing::ServiceElementIdentifierView service_2_event{
        "/score/ncar/services/TireTemperatureService", "CurrentTemperatureFrontLeft", ServiceElementType::EVENT};
    tracing::ServiceElementIdentifierView service_2_field{
        "/score/ncar/services/TireTemperatureService", "CurrentTemperatureFrontRight", ServiceElementType::FIELD};

    const auto service_1_instance_specifier =
        InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
    const auto service_2_instance_specifier =
        InstanceSpecifier::Create(std::string{"abc/abc/TireTemperaturePort"}).value();
    const auto service_1_instance_specifier_string_view = service_1_instance_specifier.ToString();
    const auto service_2_instance_specifier_string_view = service_2_instance_specifier.ToString();

    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_event, service_1_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_1_field, service_1_instance_specifier_string_view));
    EXPECT_TRUE(
        tracing_config.IsServiceElementTracingEnabled(service_2_event, service_2_instance_specifier_string_view));
    EXPECT_FALSE(
        tracing_config.IsServiceElementTracingEnabled(service_2_field, service_2_instance_specifier_string_view));
}

TEST_P(TracingFilterConfigGetNumberOfTraceingSlots, CorrectlyParseAJsonContainingNumberOfTracingSlotsInRange)
{
    const std::string instance_specifier_str{"abc/abc/TirePressurePort"};
    const std::string field_name_str{"CurrentTemperatureFrontLeft"};
    constexpr std::uint8_t number_of_tracing_slots{255};

    // Given a configuration containing a numberOfIpcTracingSlots which fits in its capacity
    // When it is parsed into the configuration
    auto config = Parse(GetParam(), "tracing_slots_255");

    const auto instance_specifier = InstanceSpecifier::Create(std::string{instance_specifier_str}).value();
    const auto& serv_inst_depl = config.GetServiceInstanceDeployment(instance_specifier).value().get();
    const auto lola_service_instance_depl = std::get<0>(serv_inst_depl.bindingInfo_);
    const auto& field = lola_service_instance_depl.fields_.at(field_name_str);

    // Then the retreived Number of tracing slots matches the original number.
    EXPECT_EQ(number_of_tracing_slots, field.lola_event_instance_deployment_.GetNumberOfTracingSlots());
}

// JSON only: flatc rejects a numberOfIpcTracingSlots which does not fit into an ubyte.
TEST_F(ConfigurationJsonOnlyParsingTest, FailParsingAJsonContainingNumberOfTracingSlotsOutOfRange)
{
    // Given a configuration containing a numberOfIpcTracingSlots which does not fit in its capacity, but is otherwise
    // valid
    // Then Expect parsing to fail
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ParseJson("tracing_slots_256"));
}

TEST_P(ConfigurationParsingStrategyTest, DuplicateServiceInstanceEventsWillDie)
{
    // Given a JSON with duplicate event

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_instance_event_name"));
}

TEST_P(ConfigurationParsingStrategyTest, DuplicateServiceInstanceFieldWillDie)
{
    // Given a JSON with duplicate Field

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_instance_field_name"));
}

TEST_P(ConfigurationParsingStrategyTest, SpecifyingServiceInstanceFieldWhichCorrespondToAServiceTypeFieldWillNotDie)
{
    // configuration is testing the positive case of the test above.
    // Given a JSON with a single known field

    // When parsing the JSON
    // That the application will not terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_NOT_VIOLATED(Parse(GetParam(), "single_instance_field"));
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, UnknownShmSizeCalcModeKeyWillDie)
{
    // Given a JSON with invalid shm size calcMode key

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ParseJson("shm_size_calc_mode_unknown"));
}

TEST_P(ConfigurationParsingStrategyTest, WithoutServiceinstancesWillDie)
{
    // Given a JSON without necessary service instance

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "service_types_without_service_instances"));
}

TEST_P(ConfigurationParsingStrategyTest, WithServiceinstancesWillNotDie)
{
    // configuration is the same as the test above and is testing the positive case.
    // Given a JSON with necessary service instance

    // When parsing the JSON
    // That the application will not terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_NOT_VIOLATED(
        Parse(GetParam(), "service_types_with_empty_service_instances"));
}

TEST_P(ConfigurationParsingStrategyTest, EmptyInstanceSpecifierWillDie)
{
    // Given a JSON with empty instance specifier

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "empty_instance_specifier"));
}

TEST_P(ConfigurationParsingStrategyTest, InvalidServiceInstanceSpecifierWillDie)
{
    // Given a JSON with invalid instance specifier

    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "invalid_instance_specifier"));
}

TEST_P(ConfigurationParsingStrategyTest, WithServiceTypeFieldsOrEventsWillNotDie)
{
    // configuration is testing the positive case of the two tests above.
    // Given a JSON with a valid instance specifier and service type fields or events
    // When parsing the JSON
    // That the application will not terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_NOT_VIOLATED(Parse(GetParam(), "valid_instance_specifier"));
}

TEST_P(ConfigurationParsingStrategyTest, DuplicateServiceInstancesWillDie)
{
    // Given a JSON with two service instances with same instanceSpecifier
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "duplicate_service_instance"));
}

TEST_P(ConfigurationParsingStrategyTest, MissingServiceTypeVersionWillDie)
{
    // Given a JSON with a service type without `version`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "missing_service_type_version"));
}

TEST_P(ConfigurationParsingStrategyTest, MissingServiceTypeMajorVersionWillDie)
{
    // Given a JSON with a service type version without `major`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "missing_service_type_major_version"));
}

TEST_P(ConfigurationParsingStrategyTest, MissingServiceInstanceMinorVersionWillDie)
{
    // Given a JSON with a service instance version without `minor`
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "missing_service_instance_minor_version"));
}

TEST_P(ConfigurationParsingStrategyTest, ValidServiceTypeVersionWillNotDie)
{
    // configuration is testing the positive case of the three tests above.
    // Given a JSON with valid service type and service instance versions

    // When parsing the JSON
    // That the application will not terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_NOT_VIOLATED(Parse(GetParam(), "single_instance_event"));
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, ParseWithMalformedServiceInstancesStructureCausesTermination)
{
    // Given a malformed JSON where serviceInstances is not an array

    // When parsing the JSON
    // Then it shall issue a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        { score::cpp::ignore = ParseJson("malformed_service_instances_not_a_list"); });
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, ParseWithMalformedInstanceSpecifierCausesTermination)
{
    // Given a malformed JSON where instance specifier is a number instead of a string

    // When parsing the JSON
    // Then it shall issue a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        { score::cpp::ignore = ParseJson("malformed_instance_specifier_not_a_string"); });
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, ParseWithMalformedServiceTypeNameCausesTermination)
{
    // Given a malformed JSON where service type name is a number instead of a string

    // When parsing the JSON
    // Then it shall issue a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        { score::cpp::ignore = ParseJson("malformed_service_type_name_not_a_string"); });
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, ParseWithMalformedVersionObjectCausesTermination)
{
    // Given a malformed JSON where version is a string instead of an object

    // When parsing the JSON
    // Then it shall issue a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        { score::cpp::ignore = ParseJson("malformed_version_not_an_object"); });
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, ParseWithMalformedDeploymentInstanceCausesTermination)
{
    // Given a malformed JSON where instances is an array of strings instead of objects

    // When parsing the JSON
    // Then it shall issue a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        { score::cpp::ignore = ParseJson("malformed_deployment_instance_not_an_object"); });
}

TEST_P(ConfigurationParsingStrategyTest, ParseWithMalformedAsilLevelCausesTermination)
{
    // Given a malformed JSON where asil level is a number instead of a string

    // When parsing the JSON
    // Then it shall issue a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        { score::cpp::ignore = Parse(GetParam(), "malformed_asil_level_not_a_string"); });
}

TEST_P(ConfigurationParsingStrategyTest, ParseWithMalformedShmSizeCalcModeHandledGracefully)
{
    // Given a malformed JSON where shm-size-calc-mode is a number instead of a string

    // When parsing the JSON
    // Then it shall issue a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        { score::cpp::ignore = Parse(GetParam(), "malformed_shm_size_calc_mode_not_a_string"); });
}

// JSON only: flatc rejects this configuration.
TEST_F(ConfigurationJsonOnlyParsingTest, ParseWithMalformedAllowedUserHandledGracefully)
{
    // Given a malformed JSON where allowedConsumer is a string instead of an object

    // When parsing the JSON
    // Then it shall issue a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        { score::cpp::ignore = ParseJson("malformed_allowed_consumer_not_an_object"); });
}

TEST_P(ConfigurationParsingStrategyTest, ParseWithMalformedPermissionChecksHandledGracefully)
{
    // Given a malformed JSON where permission-checks is a number instead of an object

    // When parsing the JSON
    // Then it shall issue a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        { score::cpp::ignore = Parse(GetParam(), "malformed_permission_checks_not_a_string"); });
}

TEST_P(ConfigurationParsingStrategyDeathTest, InvalidInterVmConfigurationWillDie)
{
    // Given a JSON where service instance is configured to be inter-VM forwarded but is not configured to have
    // interVM support, which is invalid
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        score::cpp::ignore = Parse(GetParam(), "inter_vm_forwarded_without_inter_vm_support"));
}

TEST_P(ConfigurationParsingStrategyDeathTest, NoInterVmSupportWillNotDie)
{
    // Given a JSON where service instance is not configured to have inter VM support is fine
    // When parsing the JSON
    // That the application will not terminate
    score::cpp::ignore = Parse(GetParam(), "no_inter_vm_support");
}

TEST_P(ConfigurationParsingStrategyDeathTest, InterVmSupportButNotInterVmForwardedWillNotDie)
{
    // Given a JSON where service instance is configured for inter VM support but will not forward it via inter VM is
    // fine
    // When parsing the JSON
    // That the application will not terminate
    score::cpp::ignore = Parse(GetParam(), "inter_vm_support_without_inter_vm_forwarded");
}

TEST_P(ConfigurationParsingStrategyTest, OnlyBReceiverQueueSizes)
{
    // Given a JSON with only B-receiver queue size being explicitly configured
    // When parsing the JSON
    const auto config = Parse(GetParam(), "queue_size_only_b_receiver");
    // expect that the QM-receiver has the default value
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_QM),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE);
    // and that the B-receiver has the configured value
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_B), 5);
    // and that the not explicitly configured B-sender has the default value
    EXPECT_EQ(config.GetGlobalConfiguration().GetSenderMessageQueueSize(),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_TX_QUEUE);
}

TEST_P(ConfigurationParsingStrategyTest, OnlyBSenderQueueSize)
{
    // Given a JSON with only B-sender queue size being explicitly configured
    // When parsing the JSON
    const auto config = Parse(GetParam(), "queue_size_only_b_sender");
    // expect that the QM-receiver has the default value
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_QM),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE);
    // and that the B-receiver has the default value
    EXPECT_EQ(config.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_B),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE);
    // and that the B-sender has the configured value
    EXPECT_EQ(config.GetGlobalConfiguration().GetSenderMessageQueueSize(), 12);
}

TEST_P(ConfigurationParsingStrategyTest, MultipleServiceInstancesParseSuccessfully)
{
    // Given a JSON with two valid service instances referencing the same service type
    // When parsing the JSON
    // That the application will not terminate and both instances are present
    const auto config = Parse(GetParam(), "multiple_service_instances");
    EXPECT_EQ(config.GetNumberOfServiceInstances(), 2U);
}

TEST_P(ConfigurationParsingStrategyTest, ServiceInstanceWithMultipleEventsAndFieldsParseSuccessfully)
{
    // Given a JSON with a service instance containing two events and two fields
    // When parsing the JSON
    // That the application will not terminate
    const auto config = Parse(GetParam(), "multiple_events_and_fields");
    const auto& deployment =
        config.GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value())
            .value()
            .get();
    const auto deploymentInfo = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    EXPECT_EQ(deploymentInfo.events_.size(), 2U);
    EXPECT_EQ(deploymentInfo.fields_.size(), 2U);
}

TEST_P(ConfigurationParsingStrategyTest, MultipleDeploymentsPerServiceInstanceWillDie)
{
    // Given a JSON with a service instance which has two deployments in its `instances` list
    // When parsing the JSON
    // That the application will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(Parse(GetParam(), "multiple_deployments_per_instance"));
}

TEST_P(ConfigurationParsingStrategyTest, MaximumNestingParsesEveryLevel)
{
    // Given a JSON which populates every level of the configuration with more than one entry: two service types
    // (SHM and SOME/IP) with two events, fields and methods each, two service instances with all optional
    // attributes, and the global and tracing sections
    // When parsing the JSON
    const auto config = Parse(GetParam(), "maximum_nesting");

    // Then all service types and service instances are present
    EXPECT_EQ(config.GetNumberOfServiceTypes(), 2U);
    EXPECT_EQ(config.GetNumberOfServiceInstances(), 2U);

    // And the SHM service type deployment contains all its service elements
    const auto pressure_service = make_ServiceIdentifierType("/score/ncar/services/TirePressureService", 12U, 34U);
    const auto& lola_service_type_deployment = std::get<LolaServiceTypeDeployment>(
        config.GetServiceTypeDeployment(pressure_service).value().get().binding_info_);
    EXPECT_EQ(lola_service_type_deployment.service_id_, 1234U);
    EXPECT_EQ(lola_service_type_deployment.events_.size(), 2U);
    EXPECT_EQ(lola_service_type_deployment.fields_.size(), 2U);
    EXPECT_EQ(lola_service_type_deployment.methods_.size(), 2U);
    EXPECT_EQ(lola_service_type_deployment.events_.at("CurrentPressureFrontRight"), 21U);
    EXPECT_EQ(lola_service_type_deployment.fields_.at("CurrentTemperatureFrontRight"), 31U);
    EXPECT_EQ(lola_service_type_deployment.methods_.at("ResetPressure"), 41U);

    // And the SOME/IP service type deployment contains all its service elements
    const auto temperature_service = make_ServiceIdentifierType("/score/ncar/services/TireTemperatureService", 1U, 2U);
    const auto& someip_service_type_deployment = std::get<SomeIpServiceTypeDeployment>(
        config.GetServiceTypeDeployment(temperature_service).value().get().binding_info_);
    EXPECT_EQ(someip_service_type_deployment.service_id_, 5678U);
    EXPECT_EQ(someip_service_type_deployment.events_.size(), 2U);
    EXPECT_EQ(someip_service_type_deployment.fields_.size(), 2U);
    EXPECT_EQ(someip_service_type_deployment.methods_.size(), 2U);
    EXPECT_EQ(someip_service_type_deployment.methods_.at("SelfTest"), 71U);

    // And the SHM service instance deployment contains all its optional attributes and service elements
    const auto pressure_instance_specifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
    const auto& pressure_deployment = config.GetServiceInstanceDeployment(pressure_instance_specifier).value().get();
    EXPECT_EQ(pressure_deployment.service_, pressure_service);
    EXPECT_EQ(pressure_deployment.asilLevel_, QualityType::kASIL_B);
    const auto& lola_deployment = std::get<LolaServiceInstanceDeployment>(pressure_deployment.bindingInfo_);
    EXPECT_EQ(lola_deployment.instance_id_.value(), LolaServiceInstanceId{1234U});
    EXPECT_EQ(lola_deployment.shared_memory_size_.value(), 10000U);
    EXPECT_EQ(lola_deployment.control_asil_b_memory_size_.value(), 20000U);
    EXPECT_EQ(lola_deployment.control_qm_memory_size_.value(), 30000U);
    EXPECT_TRUE(lola_deployment.strict_permissions_);
    EXPECT_TRUE(lola_deployment.inter_vm_support_);
    EXPECT_TRUE(lola_deployment.inter_vm_forwarded_);
    ASSERT_EQ(lola_deployment.allowed_consumer_.at(QualityType::kASIL_B).size(), 2U);
    EXPECT_EQ(lola_deployment.allowed_consumer_.at(QualityType::kASIL_B)[1], 55U);
    ASSERT_EQ(lola_deployment.allowed_provider_.at(QualityType::kASIL_B).size(), 1U);
    EXPECT_EQ(lola_deployment.allowed_provider_.at(QualityType::kASIL_B)[0], 16U);
    EXPECT_EQ(lola_deployment.events_.size(), 2U);
    EXPECT_EQ(lola_deployment.fields_.size(), 2U);
    EXPECT_EQ(lola_deployment.methods_.size(), 2U);
    EXPECT_EQ(lola_deployment.events_.at("CurrentPressureFrontRight").max_subscribers_.value(), 6U);
    EXPECT_TRUE(lola_deployment.fields_.at("CurrentTemperatureFrontLeft").use_get_if_available_);
    EXPECT_FALSE(lola_deployment.fields_.at("CurrentTemperatureFrontLeft").use_set_if_available_);
    EXPECT_EQ(lola_deployment.methods_.at("SetPressure").queue_size_.value(), 20U);
    EXPECT_TRUE(lola_deployment.methods_.at("SetPressure").enabled_);
    EXPECT_EQ(lola_deployment.methods_.at("ResetPressure").queue_size_.value(), 21U);
    EXPECT_FALSE(lola_deployment.methods_.at("ResetPressure").enabled_);

    // And the SOME/IP service instance deployment contains all its service elements
    const auto& temperature_deployment =
        config
            .GetServiceInstanceDeployment(InstanceSpecifier::Create(std::string{"abc/abc/TireTemperaturePort"}).value())
            .value()
            .get();
    EXPECT_EQ(temperature_deployment.service_, temperature_service);
    const auto& someip_deployment = std::get<SomeIpServiceInstanceDeployment>(temperature_deployment.bindingInfo_);
    EXPECT_EQ(someip_deployment.instance_id_.value().GetId(), 99U);
    EXPECT_EQ(someip_deployment.events_.size(), 2U);
    EXPECT_EQ(someip_deployment.fields_.size(), 2U);

    // And the global and tracing sections are parsed
    EXPECT_EQ(config.GetGlobalConfiguration().GetProcessAsilLevel(), QualityType::kASIL_B);
    EXPECT_EQ(config.GetGlobalConfiguration().GetApplicationId(), 4321U);
    EXPECT_EQ(config.GetGlobalConfiguration().GetSenderMessageQueueSize(), 12);
    const auto& tracing_config = config.GetTracingConfiguration();
    EXPECT_TRUE(tracing_config.IsTracingEnabled());
    EXPECT_EQ(tracing_config.GetApplicationInstanceID(), "maximum_nesting");
    const tracing::ServiceElementIdentifierView traced_event{
        "/score/ncar/services/TirePressureService", "CurrentPressureFrontLeft", ServiceElementType::EVENT};
    const tracing::ServiceElementIdentifierView untraced_event{
        "/score/ncar/services/TirePressureService", "CurrentPressureFrontRight", ServiceElementType::EVENT};
    EXPECT_TRUE(tracing_config.IsServiceElementTracingEnabled(traced_event, pressure_instance_specifier.ToString()));
    EXPECT_FALSE(tracing_config.IsServiceElementTracingEnabled(untraced_event, pressure_instance_specifier.ToString()));
}
}  // namespace
}  // namespace score::mw::com::impl
