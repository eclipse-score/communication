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
#include "score/mw/com/impl/configuration/mw_com_config_generated_patched.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/tracing/configuration/service_element_identifier_view.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace score::mw::com::impl::configuration
{
namespace
{

using ::score::mw::com::impl::InstanceSpecifier;
using ::score::mw::com::impl::LolaServiceInstanceDeployment;
using ::score::mw::com::impl::LolaServiceInstanceId;
using ::score::mw::com::impl::LolaServiceTypeDeployment;
using ::score::mw::com::impl::QualityType;
using ::score::mw::com::impl::ServiceElementType;

const std::string kExampleJson{"example/mw_com_config.json"};
const std::string kExampleBin{"converter/mw_com_config.bin"};
const std::string kOptionalScalarsBin{"mw_com_config_optional_scalars.bin"};

// Mirrors ConfigurationJsonParsingStrategyFixture::get_path(): tests may run either from the
// workspace root or from within an external repository's runfiles tree.
std::string GetPath(const std::string& relative_path)
{
    const std::string default_path = "score/mw/com/impl/configuration/" + relative_path;

    std::ifstream file(default_path, std::ios::binary);
    if (file.is_open())
    {
        return default_path;
    }
    return "external/safe_posix_platform/" + default_path;
}

std::vector<std::uint8_t> ReadBinaryFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    EXPECT_TRUE(file.is_open()) << "Could not open " << path;
    const auto size = file.tellg();
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

class ConfigurationFlatbufferParsingStrategyFixture : public ::testing::Test
{
};

// The headline test: the same source configuration (example/mw_com_config.json), taken down two
// completely independent parsers -- ConfigurationJsonParsingStrategy over the JSON, and
// ConfigurationFlatbufferParsingStrategy over converter:mw_com_config's FlatBuffer conversion of the
// very same JSON -- must land on an equivalent in-memory Configuration. This is the real proof that
// the two strategies are interchangeable.
TEST_F(ConfigurationFlatbufferParsingStrategyFixture, ParsingFlatbufferBinaryMatchesParsingEquivalentJson)
{
    const auto json_config = ConfigurationJsonParsingStrategy{}.Parse(GetPath(kExampleJson));
    const auto fbs_config = ConfigurationFlatbufferParsingStrategy{}.Parse(GetPath(kExampleBin));

    const auto instance_specifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();

    const auto& json_deployment = json_config.GetServiceInstanceDeployment(instance_specifier).value().get();
    const auto& fbs_deployment = fbs_config.GetServiceInstanceDeployment(instance_specifier).value().get();

    // ServiceInstanceDeployment::operator== compares the service identifier, the full LoLa binding
    // (including all events/fields/methods, allow-lists, shm sizes, inter-VM flags...), the ASIL
    // level and the instance specifier -- i.e. everything the FlatBuffer parser had to reconstruct.
    EXPECT_EQ(json_deployment, fbs_deployment);

    const auto& json_type_deployment = json_config.GetServiceTypeDeployment(json_deployment.service_).value().get();
    const auto& fbs_type_deployment = fbs_config.GetServiceTypeDeployment(fbs_deployment.service_).value().get();
    EXPECT_EQ(json_type_deployment, fbs_type_deployment);

    const auto& json_global = json_config.GetGlobalConfiguration();
    const auto& fbs_global = fbs_config.GetGlobalConfiguration();
    EXPECT_EQ(json_global.GetProcessAsilLevel(), fbs_global.GetProcessAsilLevel());
    EXPECT_EQ(json_global.GetReceiverMessageQueueSize(QualityType::kASIL_QM),
              fbs_global.GetReceiverMessageQueueSize(QualityType::kASIL_QM));
    EXPECT_EQ(json_global.GetReceiverMessageQueueSize(QualityType::kASIL_B),
              fbs_global.GetReceiverMessageQueueSize(QualityType::kASIL_B));
    EXPECT_EQ(json_global.GetSenderMessageQueueSize(), fbs_global.GetSenderMessageQueueSize());
    EXPECT_EQ(json_global.GetApplicationId(), fbs_global.GetApplicationId());
    EXPECT_EQ(json_global.GetShmSizeCalcMode(), fbs_global.GetShmSizeCalcMode());

    const auto& json_tracing = json_config.GetTracingConfiguration();
    const auto& fbs_tracing = fbs_config.GetTracingConfiguration();
    EXPECT_EQ(json_tracing.IsTracingEnabled(), fbs_tracing.IsTracingEnabled());
    EXPECT_EQ(json_tracing.GetApplicationInstanceID(), fbs_tracing.GetApplicationInstanceID());
    EXPECT_EQ(json_tracing.GetTracingFilterConfigPath(), fbs_tracing.GetTracingFilterConfigPath());

    // example/mw_com_config.json enables tracing and sets numberOfIpcTracingSlots: 7 on
    // CurrentTemperatureFrontLeft, so both parsers must have wired up per-element tracing identically.
    const tracing::ServiceElementIdentifierView field_view{
        json_deployment.service_.ToString(), "CurrentTemperatureFrontLeft", ServiceElementType::FIELD};
    EXPECT_TRUE(json_tracing.IsServiceElementTracingEnabled(field_view, instance_specifier.ToString()));
    EXPECT_EQ(json_tracing.IsServiceElementTracingEnabled(field_view, instance_specifier.ToString()),
              fbs_tracing.IsServiceElementTracingEnabled(field_view, instance_specifier.ToString()));
}

// Parse(path) loads the buffer into a local std::vector that is destroyed once Parse() returns.
// If any parsed string were left as a view into that buffer rather than a copy, it would read as
// garbage or crash under a sanitizer here.
TEST_F(ConfigurationFlatbufferParsingStrategyFixture, ParsedStringsOutliveTheSourceBuffer)
{
    const auto config = ConfigurationFlatbufferParsingStrategy{}.Parse(GetPath(kExampleBin));

    const auto instance_specifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
    const auto& deployment = config.GetServiceInstanceDeployment(instance_specifier).value().get();

    EXPECT_EQ(deployment.service_.ToString(), "/score/ncar/services/TirePressureService");

    const auto& lola_deployment = std::get<LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    ASSERT_TRUE(lola_deployment.ContainsEvent("CurrentPressureFrontLeft"));
    ASSERT_TRUE(lola_deployment.ContainsField("CurrentTemperatureFrontLeft"));
    ASSERT_TRUE(lola_deployment.ContainsMethod("SetPressure"));
}

// mw_com_config_optional_scalars.json/.bin exercises the "= null" scalars introduced in
// mw_com_config.fbs specifically to distinguish "absent" from "present and equal to a legal value
// that happens to be the type's zero": an instance with no instanceId key must parse to
// std::nullopt, while one with "instanceId": 0 must parse to a present LolaServiceInstanceId{0}.
// The same distinction is exercised for the top-level global.applicationID.
TEST_F(ConfigurationFlatbufferParsingStrategyFixture, OptionalScalarsDistinguishAbsentFromZero)
{
    const auto config = ConfigurationFlatbufferParsingStrategy{}.Parse(GetPath(kOptionalScalarsBin));

    const auto without_id_specifier = InstanceSpecifier::Create(std::string{"test/WithoutInstanceId"}).value();
    const auto& without_id_deployment = config.GetServiceInstanceDeployment(without_id_specifier).value().get();
    const auto& without_id_lola = std::get<LolaServiceInstanceDeployment>(without_id_deployment.bindingInfo_);
    EXPECT_FALSE(without_id_lola.instance_id_.has_value());

    const auto zero_id_specifier = InstanceSpecifier::Create(std::string{"test/WithZeroInstanceId"}).value();
    const auto& zero_id_deployment = config.GetServiceInstanceDeployment(zero_id_specifier).value().get();
    const auto& zero_id_lola = std::get<LolaServiceInstanceDeployment>(zero_id_deployment.bindingInfo_);
    ASSERT_TRUE(zero_id_lola.instance_id_.has_value());
    EXPECT_EQ(zero_id_lola.instance_id_.value(), LolaServiceInstanceId{0U});

    ASSERT_TRUE(config.GetGlobalConfiguration().GetApplicationId().has_value());
    EXPECT_EQ(config.GetGlobalConfiguration().GetApplicationId().value(), 0U);
}

// Required scalars of mw_com_config.fbs. FlatBuffers cannot mark scalars as required, so they are declared
// "= null": an omitted one must terminate (like the JSON parser does) instead of silently reading back as the
// type's default (0 for ids/versions, the first enum value for asil-level/binding).
enum class RequiredScalar : std::uint8_t
{
    kNone,
    kServiceTypeVersionMajor,
    kServiceTypeVersionMinor,
    kServiceTypeBinding,
    kServiceId,
    kEventId,
    kFieldId,
    kMethodId,
    kServiceInstanceVersionMajor,
    kServiceInstanceVersionMinor,
    kServiceInstanceAsilLevel,
    kServiceInstanceBinding,
};

// Builds a minimal valid configuration (one service type with an event, a field and a method, and one service
// instance of it), leaving out only the given required scalar.
std::vector<std::uint8_t> BuildConfigurationWithout(const RequiredScalar omitted)
{
    ::flatbuffers::FlatBufferBuilder fbb{};

    const auto build_version = [&fbb, omitted](const RequiredScalar major, const RequiredScalar minor) {
        fbs::ServiceVersionBuilder version{fbb};
        if (omitted != major)
        {
            version.add_major(1U);
        }
        if (omitted != minor)
        {
            version.add_minor(0U);
        }
        return version.Finish();
    };

    const auto event_name = fbb.CreateString("Event");
    fbs::ServiceTypeEventBuilder event{fbb};
    event.add_eventName(event_name);
    if (omitted != RequiredScalar::kEventId)
    {
        event.add_eventId(1U);
    }
    const auto events = fbb.CreateVector(std::vector<::flatbuffers::Offset<fbs::ServiceTypeEvent>>{event.Finish()});

    const auto field_name = fbb.CreateString("Field");
    fbs::ServiceTypeFieldBuilder field{fbb};
    field.add_fieldName(field_name);
    if (omitted != RequiredScalar::kFieldId)
    {
        field.add_fieldId(2U);
    }
    const auto fields = fbb.CreateVector(std::vector<::flatbuffers::Offset<fbs::ServiceTypeField>>{field.Finish()});

    const auto method_name = fbb.CreateString("Method");
    fbs::ServiceTypeMethodBuilder method{fbb};
    method.add_methodName(method_name);
    if (omitted != RequiredScalar::kMethodId)
    {
        method.add_methodId(3U);
    }
    const auto methods =
        fbb.CreateVector(std::vector<::flatbuffers::Offset<fbs::ServiceTypeMethod>>{method.Finish()});

    fbs::ServiceTypeBindingBuilder type_binding{fbb};
    if (omitted != RequiredScalar::kServiceTypeBinding)
    {
        type_binding.add_binding(fbs::Binding::SHM);
    }
    if (omitted != RequiredScalar::kServiceId)
    {
        type_binding.add_serviceId(1U);
    }
    type_binding.add_events(events);
    type_binding.add_fields(fields);
    type_binding.add_methods(methods);
    const auto type_bindings =
        fbb.CreateVector(std::vector<::flatbuffers::Offset<fbs::ServiceTypeBinding>>{type_binding.Finish()});

    const auto type_version =
        build_version(RequiredScalar::kServiceTypeVersionMajor, RequiredScalar::kServiceTypeVersionMinor);
    const auto type_name = fbb.CreateString("/test/Service");
    fbs::ServiceTypeBuilder service_type{fbb};
    service_type.add_serviceTypeName(type_name);
    service_type.add_version(type_version);
    service_type.add_bindings(type_bindings);
    const auto service_types =
        fbb.CreateVector(std::vector<::flatbuffers::Offset<fbs::ServiceType>>{service_type.Finish()});

    fbs::ServiceInstanceBindingBuilder instance_binding{fbb};
    if (omitted != RequiredScalar::kServiceInstanceAsilLevel)
    {
        instance_binding.add_asil_level(fbs::AsilLevel::QM);
    }
    if (omitted != RequiredScalar::kServiceInstanceBinding)
    {
        instance_binding.add_binding(fbs::Binding::SHM);
    }
    const auto instance_bindings = fbb.CreateVector(
        std::vector<::flatbuffers::Offset<fbs::ServiceInstanceBinding>>{instance_binding.Finish()});

    const auto instance_version =
        build_version(RequiredScalar::kServiceInstanceVersionMajor, RequiredScalar::kServiceInstanceVersionMinor);
    const auto instance_specifier = fbb.CreateString("test/Instance");
    const auto instance_type_name = fbb.CreateString("/test/Service");
    fbs::ServiceInstanceBuilder service_instance{fbb};
    service_instance.add_instanceSpecifier(instance_specifier);
    service_instance.add_serviceTypeName(instance_type_name);
    service_instance.add_version(instance_version);
    service_instance.add_instances(instance_bindings);
    const auto service_instances =
        fbb.CreateVector(std::vector<::flatbuffers::Offset<fbs::ServiceInstance>>{service_instance.Finish()});

    fbs::ConfigurationBuilder configuration{fbb};
    configuration.add_serviceTypes(service_types);
    configuration.add_serviceInstances(service_instances);
    fbs::FinishConfigurationBuffer(fbb, configuration.Finish());

    return {fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize()};
}

// Proves BuildConfigurationWithout() yields a valid configuration, so that in RequiredScalarTest the omitted
// scalar is the only reason for termination.
TEST_F(ConfigurationFlatbufferParsingStrategyFixture, ConfigurationWithAllRequiredScalarsParses)
{
    const auto buffer = BuildConfigurationWithout(RequiredScalar::kNone);

    const auto config = ConfigurationFlatbufferParsingStrategy{}.Parse(score::cpp::span<const std::uint8_t>{buffer});

    const auto instance_specifier = InstanceSpecifier::Create(std::string{"test/Instance"}).value();
    const auto& deployment = config.GetServiceInstanceDeployment(instance_specifier).value().get();
    EXPECT_EQ(deployment.asilLevel_, QualityType::kASIL_QM);
    EXPECT_EQ(deployment.service_, make_ServiceIdentifierType("/test/Service", 1U, 0U));
}

class RequiredScalarTest : public ::testing::TestWithParam<RequiredScalar>
{
};

TEST_P(RequiredScalarTest, MissingRequiredScalarWillDie)
{
    const auto buffer = BuildConfigurationWithout(GetParam());

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        ConfigurationFlatbufferParsingStrategy{}.Parse(score::cpp::span<const std::uint8_t>{buffer}));
}

INSTANTIATE_TEST_SUITE_P(ConfigurationFlatbufferParsingStrategy,
                         RequiredScalarTest,
                         ::testing::Values(RequiredScalar::kServiceTypeVersionMajor,
                                           RequiredScalar::kServiceTypeVersionMinor,
                                           RequiredScalar::kServiceTypeBinding,
                                           RequiredScalar::kServiceId,
                                           RequiredScalar::kEventId,
                                           RequiredScalar::kFieldId,
                                           RequiredScalar::kMethodId,
                                           RequiredScalar::kServiceInstanceVersionMajor,
                                           RequiredScalar::kServiceInstanceVersionMinor,
                                           RequiredScalar::kServiceInstanceAsilLevel,
                                           RequiredScalar::kServiceInstanceBinding));

struct QueueSizes
{
    std::uint32_t qm_receiver;
    std::uint32_t b_receiver;
    std::uint32_t b_sender;
};

// Builds a configuration without any service types/instances, carrying only the given global properties.
std::vector<std::uint8_t> BuildConfigurationWithGlobal(const std::optional<std::uint32_t> application_id,
                                                       const std::optional<QueueSizes> queue_sizes)
{
    ::flatbuffers::FlatBufferBuilder fbb{};

    ::flatbuffers::Offset<fbs::QueueSize> queue_size{};
    if (queue_sizes.has_value())
    {
        queue_size = fbs::CreateQueueSize(
            fbb, queue_sizes.value().qm_receiver, queue_sizes.value().b_receiver, queue_sizes.value().b_sender);
    }

    fbs::GlobalConfigurationBuilder global{fbb};
    if (application_id.has_value())
    {
        global.add_applicationID(application_id.value());
    }
    if (queue_sizes.has_value())
    {
        global.add_queue_size(queue_size);
    }
    const auto global_offset = global.Finish();

    const auto service_types = fbb.CreateVector(std::vector<::flatbuffers::Offset<fbs::ServiceType>>{});
    const auto service_instances = fbb.CreateVector(std::vector<::flatbuffers::Offset<fbs::ServiceInstance>>{});
    fbs::ConfigurationBuilder configuration{fbb};
    configuration.add_serviceTypes(service_types);
    configuration.add_serviceInstances(service_instances);
    configuration.add_global(global_offset);
    fbs::FinishConfigurationBuffer(fbb, configuration.Finish());

    return {fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize()};
}

// Like the JSON parser, the maximum ApplicationId is rejected (it is reserved as "invalid").
TEST_F(ConfigurationFlatbufferParsingStrategyFixture, MaxApplicationIdWillDie)
{
    const auto buffer = BuildConfigurationWithGlobal(std::numeric_limits<std::uint32_t>::max(), std::nullopt);

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        ConfigurationFlatbufferParsingStrategy{}.Parse(score::cpp::span<const std::uint8_t>{buffer}));
}

TEST_F(ConfigurationFlatbufferParsingStrategyFixture, QueueSizesUpToInt32MaxAreAccepted)
{
    constexpr auto kInt32Max = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    const auto buffer = BuildConfigurationWithGlobal(std::nullopt, QueueSizes{kInt32Max, kInt32Max, kInt32Max});

    const auto config = ConfigurationFlatbufferParsingStrategy{}.Parse(score::cpp::span<const std::uint8_t>{buffer});

    const auto& global = config.GetGlobalConfiguration();
    EXPECT_EQ(global.GetReceiverMessageQueueSize(QualityType::kASIL_QM), std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(global.GetReceiverMessageQueueSize(QualityType::kASIL_B), std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(global.GetSenderMessageQueueSize(), std::numeric_limits<std::int32_t>::max());
}

// Queue sizes are std::uint32_t in the .fbs but std::int32_t in GlobalConfiguration: a value above INT32_MAX
// must terminate (like the JSON parser does) instead of wrapping around to a negative queue size.
TEST_F(ConfigurationFlatbufferParsingStrategyFixture, QueueSizeAboveInt32MaxWillDie)
{
    constexpr std::uint32_t kValid{10U};
    constexpr auto kTooLarge = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) + 1U;

    for (const auto& queue_sizes : {QueueSizes{kTooLarge, kValid, kValid},
                                    QueueSizes{kValid, kTooLarge, kValid},
                                    QueueSizes{kValid, kValid, kTooLarge}})
    {
        const auto buffer = BuildConfigurationWithGlobal(std::nullopt, queue_sizes);

        SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
            ConfigurationFlatbufferParsingStrategy{}.Parse(score::cpp::span<const std::uint8_t>{buffer}));
    }
}

TEST_F(ConfigurationFlatbufferParsingStrategyFixture, EmptyBufferWillDie)
{
    const std::vector<std::uint8_t> empty_buffer{};

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        ConfigurationFlatbufferParsingStrategy{}.Parse(score::cpp::span<const std::uint8_t>{empty_buffer}));
}

TEST_F(ConfigurationFlatbufferParsingStrategyFixture, TruncatedBufferWillDie)
{
    auto buffer = ReadBinaryFile(GetPath(kExampleBin));
    buffer.resize(buffer.size() / 2U);

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        ConfigurationFlatbufferParsingStrategy{}.Parse(score::cpp::span<const std::uint8_t>{buffer}));
}

// Bytes [4, 8) of a FlatBuffer binary hold the file_identifier ("MWCC" for this schema, right after
// the 4-byte root table uoffset). Corrupting it must fail VerifyConfigurationBuffer's identifier
// check even though the rest of the buffer is perfectly well-formed.
TEST_F(ConfigurationFlatbufferParsingStrategyFixture, WrongFileIdentifierWillDie)
{
    auto buffer = ReadBinaryFile(GetPath(kExampleBin));
    ASSERT_GE(buffer.size(), 8U);
    for (std::size_t i = 4U; i < 8U; ++i)
    {
        buffer.at(i) = static_cast<std::uint8_t>(~buffer.at(i));
    }

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        ConfigurationFlatbufferParsingStrategy{}.Parse(score::cpp::span<const std::uint8_t>{buffer}));
}

TEST_F(ConfigurationFlatbufferParsingStrategyFixture, InvalidPathWillDie)
{
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        ConfigurationFlatbufferParsingStrategy{}.Parse(std::string_view{"my_invalid_path_to_nowhere"}));
}

}  // namespace
}  // namespace score::mw::com::impl::configuration
