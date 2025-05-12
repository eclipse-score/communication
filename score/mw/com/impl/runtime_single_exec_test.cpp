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
#include "score/filesystem/factory/filesystem_factory.h"
#include "score/mw/com/impl/runtime.h"

#include "score/memory/string_literal.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_specifier.h"

#include <score/blank.hpp>
#include <score/overload.hpp>
#include <score/span.hpp>
#include <score/string_view.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

namespace score::mw::com::impl
{
namespace
{

constexpr auto kDummyApplicationName = "dummyname";

// This fixture forces every test to run within its own process.
// This is required to support resets of a Meyer-Singleton.
// This fixture should be used sparingly, since it has quite a big runtime penalty.
// Every test must be executed within TestInSeparateProcess()!
class SingleTestPerProcessFixture : public ::testing::Test
{
  public:
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
    InstanceSpecifier tire_pressure_port_{InstanceSpecifier::Create("abc/abc/TirePressurePort").value()};
    std::string config_with_tire_pressure_port_{get_path("ara_com_config.json")};
    InstanceSpecifier tire_pressure_port_other_{InstanceSpecifier::Create("abc/abc/TirePressurePortOther").value()};
    std::string config_with_tire_pressure_port_other_{get_path("ara_com_config_other.json")};
    bool tested_in_separate_process_{false};

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

class CallArgs
{
  public:
    explicit CallArgs(std::string source_path)
        : source_path_{std::move(source_path)},
          call_args_{kDummyApplicationName, "-service_instance_manifest", source_path_.c_str()},
          contains_path_{true}
    {
    }

    explicit CallArgs() : source_path_{}, call_args_{kDummyApplicationName}, contains_path_{false} {}

    score::cpp::span<const score::StringLiteral> Get() const
    {
        if (contains_path_)
        {
            return call_args_;
        }
        return score::cpp::span<const score::StringLiteral>{&call_args_[0], 1U};
    }

  private:
    std::string source_path_;
    score::StringLiteral call_args_[3];
    bool contains_path_;
};

CallArgs WithCallArgsContainingConfigPath(std::string source_path)
{
    return CallArgs(std::move(source_path));
}

CallArgs WithCallArgsNotContainingConfigPath()
{
    return CallArgs();
}

std::vector<score::cpp::string_view> GetEventNameListFromHandle(const HandleType& handle_type) noexcept
{
    using ReturnType = std::vector<score::cpp::string_view>;

    const auto& identifier = handle_type.GetInstanceIdentifier();
    const auto& service_type_deployment = InstanceIdentifierView{identifier}.GetServiceTypeDeployment();
    auto visitor = score::cpp::overload(
        [](const LolaServiceTypeDeployment& deployment) -> ReturnType {
            ReturnType event_names;
            for (const auto& event : deployment.events_)
            {
                const auto event_name = score::cpp::string_view{event.first.data(), event.first.size()};
                event_names.push_back(event_name);
            }
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
    filesystem::Path target{"etc/mw_com_config.json"};
    ASSERT_TRUE(filesystem.CopyFile(filesystem::Path{source_path}, target).has_value());
}

std::string WithConfigInBuffer(const std::string& source_path)
{
    std::ifstream configuration{source_path};
    std::string configuration_buffer{std::istreambuf_iterator<char>{configuration}, std::istreambuf_iterator<char>{}};

    return configuration_buffer;
}

using RuntimeCallArgsInitializationTest = SingleTestPerProcessFixture;

TEST_F(RuntimeCallArgsInitializationTest, InitializationLoadsCorrectConfiguration)
{
    RecordProperty("Verifies", "SCR-6221480, SCR-21781439");
    RecordProperty("Description", "InstanceSpecifier resolution can not retrieve wrong InstanceIdentifier.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([this]() {
        // Given a configuration with the tire_pressure_port_ provided by path as call arguments
        const auto test_args = WithCallArgsContainingConfigPath(config_with_tire_pressure_port_);

        // When initializing the runtime with the call args
        Runtime::Initialize(test_args.Get());

        // Then we can resolve this instance identifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

TEST_F(RuntimeCallArgsInitializationTest, InitializationLoadsDefaultConfigurationIfNoneProvided)
{
    TestInSeparateProcess([this]() {
        // Given a configuration at the default location with the tire_pressure_port_ and call args which don't specify
        // the configuration path
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);
        const auto test_args = WithCallArgsNotContainingConfigPath();

        // When initializing the runtime with the call args
        Runtime::Initialize(test_args.Get());

        // Then we can resolve this instance identifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

TEST_F(RuntimeCallArgsInitializationTest, SecondInitializationUpdatesRuntimeIfRuntimeHasNotYetBeenUsed)
{
    TestInSeparateProcess([this]() {
        // Given two configurations with one instance specifier each
        const auto test_args_1 = WithCallArgsContainingConfigPath(config_with_tire_pressure_port_);
        const auto test_args_2 = WithCallArgsContainingConfigPath(config_with_tire_pressure_port_other_);

        // and that the runtime has been initialised with the first configuration
        Runtime::Initialize(test_args_1.Get());

        // When initializing the runtime with the second configuration before the runtime is used
        Runtime::Initialize(test_args_2.Get());

        // Then we can only resolve the second instance specifier
        auto identifiers_1 = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers_1.size(), 0);
        auto identifiers_2 = Runtime::getInstance().resolve(tire_pressure_port_other_);
        EXPECT_EQ(identifiers_2.size(), 1);
    });
}

TEST_F(RuntimeCallArgsInitializationTest, SecondInitializationDoesNotUpdateRuntimeIfRuntimeHasAlreadyBeenUsed)
{
    TestInSeparateProcess([this]() {
        // Given two configurations with one instance specifier each
        const auto test_args_1 = WithCallArgsContainingConfigPath(config_with_tire_pressure_port_);
        const auto test_args_2 = WithCallArgsContainingConfigPath(config_with_tire_pressure_port_other_);

        // and that the runtime has been initialised with the first configuration
        Runtime::Initialize(test_args_1.Get());

        // and that the runtime has been used
        score::cpp::ignore = Runtime::getInstance().resolve(tire_pressure_port_);

        // When initializing the runtime with the second configuration
        Runtime::Initialize(test_args_2.Get());

        // Then we can only resolve the first instance specifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_other_);
        EXPECT_EQ(identifiers.size(), 0);
        identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

using RuntimeDefaultInitializationTest = SingleTestPerProcessFixture;

TEST_F(RuntimeDefaultInitializationTest, ImplicitInitializationLoadsCorrectConfiguration)
{
    TestInSeparateProcess([this]() {
        // Given a configuration with one instance specifier provided at default location
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);

        // When implicitly default-initializing the runtime
        auto& runtime = Runtime::getInstance();

        // Then we can resolve this instance identifier
        auto identifiers = runtime.resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

TEST_F(RuntimeDefaultInitializationTest, ExplicitInitializationLoadsCorrectConfiguration)
{
    TestInSeparateProcess([this]() {
        // Given a configuration with one instance specifier provided at default location
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);

        // When explicitly default-initializing the runtime
        Runtime::Initialize();

        // Then we can resolve this instance identifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

TEST_F(RuntimeDefaultInitializationTest, SecondInitializationUpdatesRuntimeIfNotYetLocked)
{
    TestInSeparateProcess([this]() {
        // Given a configuration with one instance specifier

        // When initializing the runtime with the first configuration and then the second configuration
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);
        Runtime::Initialize();

        WithConfigAtDefaultPath(config_with_tire_pressure_port_other_);
        Runtime::Initialize();

        // Then we can only resolve the second instance specifier
        auto identifiers_1 = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers_1.size(), 0);
        auto identifiers_2 = Runtime::getInstance().resolve(tire_pressure_port_other_);
        EXPECT_EQ(identifiers_2.size(), 1);
    });
}

TEST_F(RuntimeDefaultInitializationTest, SecondInitializationDoesNotUpdatesRuntimeIfLocked)
{
    TestInSeparateProcess([this]() {
        // Given two configurations with one instance specifier each

        // When initializing the runtime with the first configuration and then the second configuration with a usage of
        // the runtime in between
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);
        Runtime::Initialize();

        score::cpp::ignore = Runtime::getInstance().resolve(tire_pressure_port_);

        WithConfigAtDefaultPath(config_with_tire_pressure_port_other_);
        Runtime::Initialize();

        // Then we can only resolve the first instance specifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_other_);
        EXPECT_EQ(identifiers.size(), 0);
        identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

using RuntimeInitializationFromBufferTest = SingleTestPerProcessFixture;

TEST_F(RuntimeInitializationFromBufferTest, InitializationLoadsCorrectConfiguration)
{
    TestInSeparateProcess([this]() {
        // Given a configuration with one instance specifier provided in a buffer
        const auto configuration_buffer = WithConfigInBuffer(config_with_tire_pressure_port_);

        // When initializing the runtime
        Runtime::Initialize(configuration_buffer);

        // Then we can resolve this instance identifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

TEST_F(RuntimeInitializationFromBufferTest, SecondInitializationUpdatesRuntimeIfNotYetLocked)
{
    TestInSeparateProcess([this]() {
        // Given two configurations with one instance specifier and provided in a buffer each
        const auto configuration_buffer_1 = WithConfigInBuffer(config_with_tire_pressure_port_);

        const auto configuration_buffer_2 = WithConfigInBuffer(config_with_tire_pressure_port_other_);

        // When initializing the runtime with the first configuration and then the second configuration
        Runtime::Initialize(configuration_buffer_1);
        Runtime::Initialize(configuration_buffer_2);

        // Then we can only resolve the second instance specifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 0);
        identifiers = Runtime::getInstance().resolve(tire_pressure_port_other_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

TEST_F(RuntimeInitializationFromBufferTest, SecondInitializationDoesNotUpdatesRuntimeIfLocked)
{
    TestInSeparateProcess([this]() {
        // Given two configurations with one instance specifier and provided in a buffer each
        const auto configuration_buffer_1 = WithConfigInBuffer(config_with_tire_pressure_port_);

        const auto configuration_buffer_2 = WithConfigInBuffer(config_with_tire_pressure_port_other_);

        // When initializing the runtime with the first configuration and then the second configuration with a usage of
        // the runtime in between
        Runtime::Initialize(configuration_buffer_1);
        score::cpp::ignore = Runtime::getInstance().resolve(tire_pressure_port_);
        Runtime::Initialize(configuration_buffer_2);

        // Then we can only resolve the first instance specifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_other_);
        EXPECT_EQ(identifiers.size(), 0);
        identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

using RuntimeTest = SingleTestPerProcessFixture;

TEST_F(RuntimeTest, CannotResolveUnknownInstanceSpecifier)
{
    RecordProperty("Verifies", "SCR-6221480, SCR-21781439");
    RecordProperty("Description", "InstanceSpecifier resolution can not retrieve wrong InstanceIdentifier.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([this]() {
        // Given aconfiguration without the tire_pressure_port_other_ instance specifier
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);

        // When resolving this instance specifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_other_);

        // Then no instance identifiers are returned
        EXPECT_TRUE(identifiers.empty());
    });
}

TEST_F(RuntimeTest, CanRetrieveConfiguredBinding)
{
    TestInSeparateProcess([this]() {
        // Given a configuration, which contains lola bindings
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);

        // When retrieving the lola binding
        const auto* unit = Runtime::getInstance().GetBindingRuntime(BindingType::kLoLa);

        // Then the lola binding can be retrieved
        EXPECT_NE(unit, nullptr);
    });
}

TEST_F(RuntimeTest, CannotRetrieveUnconfiguredBinding)
{
    TestInSeparateProcess([this]() {
        // Given a configuration, which does not contain Fake bindings
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);

        // When retrieving the fake binding
        const auto* unit = Runtime::getInstance().GetBindingRuntime(BindingType::kFake);

        // Then no fake binding can be retrieved
        EXPECT_EQ(unit, nullptr);
    });
}

TEST_F(RuntimeTest, HandleTypeContainsEventsSpecifiedInConfiguration)
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
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);

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

TEST_F(RuntimeTest, TracingIsDisabledWhenTraceFilterConfigPathIsInvalid)
{
    RecordProperty("Verifies", "SCR-18159104");
    RecordProperty("Description",
                   "Checks that tracing is disabled (indicated by lack of tracing runtime) when TraceFilterConfig path "
                   "does not point to a valid tracing configuration.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([]() {
        // Given a configuration file which contains a TraceFilterConfigPath that does not point to a valid tracing
        // configuration
        WithConfigAtDefaultPath(get_path("ara_com_config_invalid_trace_config_path.json"));

        // When initializing the runtime
        Runtime::Initialize();

        // Then tracing will be disabled
        EXPECT_EQ(Runtime::getInstance().GetTracingRuntime(), nullptr);
    });
}

TEST_F(RuntimeTest, TracingRuntimeIsDisabledWhenTracingDisabledInConfig)
{
    TestInSeparateProcess([]() {
        // Given a configuration with valid and disabled tracing configuration
        WithConfigAtDefaultPath(get_path("ara_com_config_disabled_trace_config.json"));

        // When initializing the runtime
        Runtime::Initialize();

        // Then tracing will be disabled
        EXPECT_EQ(Runtime::getInstance().GetTracingRuntime(), nullptr);
    });
}

TEST_F(RuntimeTest, TracingRuntimeIsCreatedIfConfiguredCorrectly)
{
    TestInSeparateProcess([]() {
        // Given a configuration with valid and enabled tracing configuration
        auto default_path = get_path("ara_com_config_valid_trace_config.json");
        auto json_path = default_path.find("external") != std::string::npos
                             ? get_path("ara_com_config_valid_trace_config_external.json")
                             : default_path;
        WithConfigAtDefaultPath(json_path);

        // When initializing the runtime
        Runtime::Initialize();

        // Then tracing runtime will exist
        EXPECT_NE(Runtime::getInstance().GetTracingRuntime(), nullptr);
    });
}

}  // namespace
}  // namespace score::mw::com::impl
