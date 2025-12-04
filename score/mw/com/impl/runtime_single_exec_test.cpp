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
#include "score/mw/com/impl/runtime.h"

#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/runtime_configuration.h"

#include "score/filesystem/factory/filesystem_factory.h"

#include <score/blank.hpp>
#include <score/overload.hpp>
#include <score/span.hpp>

#include <score/utility.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace score::mw::com::impl
{
namespace
{

// This fixture forces every test to run within its own process.
// This is required to support resets of a Meyer-Singleton.
// This fixture should be used sparingly, since it has quite a big runtime penalty.
// Every test must be executed within TestInSeparateProcess()!
class SingleTestPerProcessFixture : public ::testing::TestWithParam<std::string>
{
  public:
    void SetUp() override
    {
        config_file_extension_ = GetParam();
    }

    void TearDown() override
    {
        // Safeguard against accidentially not using TestInSeparateProcess()
        ASSERT_TRUE(tested_in_separate_process_);
    }

    template <class TestFunction>
    void TestInSeparateProcess(TestFunction test_function)
    {
        const auto exit_test = [&test_function]() {
            std::invoke(std::forward<TestFunction>(test_function));

            PrintTestPartResults();

            if (HasFailure())
            {
                std::exit(1);
            }
            std::exit(0);
        };

        EXPECT_EXIT(exit_test(), ::testing::ExitedWithCode(0), ".*");

        tested_in_separate_process_ = true;
    }

    static const std::string get_path(const std::string& file_name)
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

  protected:
    InstanceSpecifier tire_pressure_port_{InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value()};
    InstanceSpecifier tire_pressure_port_other_{
        InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePortOther"}).value()};
    bool tested_in_separate_process_{false};
    std::string config_file_extension_;

    std::string config_with_tire_pressure_port() const
    {
        return get_path("ara_com_config" + config_file_extension_);
    }

    std::string config_with_tire_pressure_port_other() const
    {
        return get_path("ara_com_config_other" + config_file_extension_);
    }

  private:
    static void PrintTestPartResults()
    {
        auto* const instance = testing::UnitTest::GetInstance();
        auto* const test_result = instance->current_test_info()->result();
        auto* const listener = instance->listeners().default_result_printer();
        for (auto i = 0; i < test_result->total_part_count(); ++i)
        {
            listener->OnTestPartResult(test_result->GetTestPartResult(i));
        }
    }
};

std::vector<std::string_view> GetEventNameListFromHandle(const HandleType& handle_type) noexcept
{
    using ReturnType = std::vector<std::string_view>;

    const auto& identifier = handle_type.GetInstanceIdentifier();
    const auto& service_type_deployment = InstanceIdentifierView{identifier}.GetServiceTypeDeployment();
    auto visitor = score::cpp::overload(
        [](const LolaServiceTypeDeployment& deployment) -> ReturnType {
            ReturnType event_names;
            std::transform(deployment.events_.cbegin(),
                           deployment.events_.cend(),
                           std::back_inserter(event_names),
                           [](const auto& event) -> std::string_view {
                               return event.first;
                           });
            return event_names;
        },
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return {};
        });
    return std::visit(visitor, service_type_deployment.binding_info_);
}

void WithConfigAtDefaultPath(const std::string& source_path)
{
    auto& filesystem = filesystem::IStandardFilesystem::instance();
    filesystem::Path dir{"etc"};
    score::cpp::ignore = filesystem.RemoveAll(dir);
    ASSERT_TRUE(filesystem.CreateDirectories(dir).has_value());

    // Determine target filename based on source file extension
    const bool is_flatbuffer = source_path.find(".bin") != std::string::npos;
    filesystem::Path target = is_flatbuffer ? filesystem::Path{"etc/mw_com_config.bin"}
                                             : filesystem::Path{"etc/mw_com_config.json"};
    ASSERT_TRUE(filesystem.CopyFile(filesystem::Path{source_path}, target).has_value());
}

using RuntimeInitializationTest = SingleTestPerProcessFixture;

TEST_P(RuntimeInitializationTest, InitializationLoadsCorrectConfiguration)
{
    RecordProperty("Verifies", "SCR-6221480, SCR-21781439");
    RecordProperty("Description", "InstanceSpecifier resolution can not retrieve wrong InstanceIdentifier.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([this]() {
        // Given a RuntimeConfiguration
        const auto runtime_configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port()};

        // When initializing the runtime with the call args
        Runtime::Initialize(runtime_configuration);

        // Then we can resolve this instance identifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

TEST_P(RuntimeInitializationTest, SecondInitializationUpdatesRuntimeIfRuntimeHasNotYetBeenUsed)
{
    TestInSeparateProcess([this]() {
        // Given two RuntimeConfigurations containing different configuration file paths
        const auto runtime_configuration_1 = runtime::RuntimeConfiguration{config_with_tire_pressure_port()};
        const auto runtime_configuration_2 = runtime::RuntimeConfiguration{config_with_tire_pressure_port_other()};

        // and that the runtime has been initialised with the first configuration
        Runtime::Initialize(runtime_configuration_1);

        // When initializing the runtime with the second configuration before the runtime is used
        Runtime::Initialize(runtime_configuration_2);

        // Then we can only resolve the second instance specifier
        auto identifiers_1 = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers_1.size(), 0);
        auto identifiers_2 = Runtime::getInstance().resolve(tire_pressure_port_other_);
        EXPECT_EQ(identifiers_2.size(), 1);
    });
}

TEST_P(RuntimeInitializationTest, SecondInitializationDoesNotUpdateRuntimeIfRuntimeHasAlreadyBeenUsed)
{
    TestInSeparateProcess([this]() {
        // Given two RuntimeConfigurations containing different configuration file paths
        const auto runtime_configuration_1 = runtime::RuntimeConfiguration{config_with_tire_pressure_port()};
        const auto runtime_configuration_2 = runtime::RuntimeConfiguration{config_with_tire_pressure_port_other()};

        // and that the runtime has been initialised with the first configuration
        Runtime::Initialize(runtime_configuration_1);

        // and that the runtime has been used
        score::cpp::ignore = Runtime::getInstance().resolve(tire_pressure_port_);

        // When initializing the runtime with the second configuration
        Runtime::Initialize(runtime_configuration_2);

        // Then we can only resolve the first instance specifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_other_);
        EXPECT_EQ(identifiers.size(), 0);
        identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

TEST_P(RuntimeInitializationTest, ImplicitInitializationLoadsCorrectConfiguration)
{
    TestInSeparateProcess([this]() {
        // Given a configuration with one instance specifier provided at default location
        WithConfigAtDefaultPath(config_with_tire_pressure_port());

        // When implicitly default-initializing the runtime
        auto& runtime = Runtime::getInstance();

        // Then we can resolve this instance identifier
        auto identifiers = runtime.resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

using RuntimeTest = SingleTestPerProcessFixture;

TEST_P(RuntimeTest, CannotResolveUnknownInstanceSpecifier)
{
    RecordProperty("Verifies", "SCR-6221480, SCR-21781439");
    RecordProperty("Description", "InstanceSpecifier resolution can not retrieve wrong InstanceIdentifier.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([this]() {
        // Given aconfiguration without the tire_pressure_port_other_ instance specifier
        WithConfigAtDefaultPath(config_with_tire_pressure_port());

        // When resolving this instance specifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_other_);

        // Then no instance identifiers are returned
        EXPECT_TRUE(identifiers.empty());
    });
}

TEST_P(RuntimeTest, CanRetrieveConfiguredBinding)
{
    TestInSeparateProcess([this]() {
        // Given a configuration, which contains lola bindings
        WithConfigAtDefaultPath(config_with_tire_pressure_port());

        // When retrieving the lola binding
        const auto* unit = Runtime::getInstance().GetBindingRuntime(BindingType::kLoLa);

        // Then the lola binding can be retrieved
        EXPECT_NE(unit, nullptr);
    });
}

TEST_P(RuntimeTest, CannotRetrieveUnconfiguredBinding)
{
    TestInSeparateProcess([this]() {
        // Given a configuration, which does not contain Fake bindings
        WithConfigAtDefaultPath(config_with_tire_pressure_port());

        // When retrieving the fake binding
        const auto* unit = Runtime::getInstance().GetBindingRuntime(BindingType::kFake);

        // Then no fake binding can be retrieved
        EXPECT_EQ(unit, nullptr);
    });
}

TEST_P(RuntimeTest, HandleTypeContainsEventsSpecifiedInConfiguration)
{
    RecordProperty("Verifies", "SCR-15600146");
    RecordProperty("Description",
                   "A HandleType containing the events in the Lola configuration file can be created from the "
                   "configuration file.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([this]() {
        // Given a configuration with an instance specifier
        WithConfigAtDefaultPath(config_with_tire_pressure_port());

        // When creating a handle from the InstanceSpecifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        ASSERT_EQ(identifiers.size(), 1);
        auto handle_type = make_HandleType(identifiers[0]);
        const auto event_name_list = GetEventNameListFromHandle(handle_type);

        // Then the handle will contain the events specified in the configuration.
        ASSERT_EQ(event_name_list.size(), 1);
        EXPECT_THAT(event_name_list, ::testing::Contains("CurrentPressureFrontLeft"));
    });
}

TEST_P(RuntimeTest, TracingIsDisabledWhenTraceFilterConfigPathIsInvalid)
{
    RecordProperty("Verifies", "SCR-18159104");
    RecordProperty("Description",
                   "Checks that tracing is disabled (indicated by lack of tracing runtime) when TraceFilterConfig path "
                   "does not point to a valid tracing configuration.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([this]() {
        // Given a configuration file which contains a TraceFilterConfigPath that does not point to a valid tracing
        // configuration
        WithConfigAtDefaultPath(get_path("ara_com_config_invalid_trace_config_path" + config_file_extension_));

        // When implicitly default-initializing the runtime
        score::cpp::ignore = Runtime::getInstance();

        // Then tracing will be disabled
        EXPECT_EQ(Runtime::getInstance().GetTracingRuntime(), nullptr);
    });
}

TEST_P(RuntimeTest, TracingRuntimeIsDisabledWhenTracingDisabledInConfig)
{
    TestInSeparateProcess([this]() {
        // Given a configuration with valid and disabled tracing configuration
        WithConfigAtDefaultPath(get_path("ara_com_config_disabled_trace_config" + config_file_extension_));

        // When implicitly default-initializing the runtime
        score::cpp::ignore = Runtime::getInstance();

        // Then tracing will be disabled
        EXPECT_EQ(Runtime::getInstance().GetTracingRuntime(), nullptr);
    });
}

TEST_P(RuntimeTest, TracingRuntimeIsCreatedIfConfiguredCorrectly)
{
    TestInSeparateProcess([this]() {
        // Given a configuration with valid and enabled tracing configuration
        auto default_path = get_path("ara_com_config_valid_trace_config" + config_file_extension_);
        auto config_path = default_path.find("external") != std::string::npos
                             ? get_path("ara_com_config_valid_trace_config_external" + config_file_extension_)
                             : default_path;
        WithConfigAtDefaultPath(config_path);

        // When implicitly default-initializing the runtime
        score::cpp::ignore = Runtime::getInstance();

        // Then tracing runtime will exist
        EXPECT_NE(Runtime::getInstance().GetTracingRuntime(), nullptr);
    });
}

INSTANTIATE_TEST_SUITE_P(
    JsonAndFlatBuffer,
    RuntimeInitializationTest,
    ::testing::Values(".json", ".bin"),
    [](const ::testing::TestParamInfo<std::string>& info) {
        return info.param == ".json" ? "Json" : "FlatBuffer";
    });

INSTANTIATE_TEST_SUITE_P(
    JsonAndFlatBuffer,
    RuntimeTest,
    ::testing::Values(".json", ".bin"),
    [](const ::testing::TestParamInfo<std::string>& info) {
        return info.param == ".json" ? "Json" : "FlatBuffer";
    });

}  // namespace
}  // namespace score::mw::com::impl
