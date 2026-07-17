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
#include "score/utils/meyer_singleton/test/single_test_per_process_fixture.h"

#include <score/blank.hpp>
#include <score/overload.hpp>
#include <score/span.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <score/utility.hpp>

#include <cstdlib>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

#include "configuration/configuration_error.h"

namespace score::mw::com::impl
{

// Test-only helper granting access to Runtime's private configuration_ member (befriended in runtime.h). Must live at
// score::mw::com::impl scope (not in an anonymous namespace) so its name matches the friend declaration.
class RuntimeAttorney
{
  public:
    explicit RuntimeAttorney(Runtime& runtime) noexcept : runtime_{runtime} {}
    const Configuration* GetConfigurationAddress() const noexcept
    {
        return &runtime_.configuration_;
    }

    bool IsAddonConfigurationLoaded() const noexcept
    {
        return Runtime::addon_configuration_loaded_;
    }

  private:
    Runtime& runtime_;
};

// Test-only helper granting read access to the static InstanceIdentifier::configuration_ pointer (befriended in
// instance_identifier.h). Must live at score::mw::com::impl scope so its name matches the friend declaration.
class InstanceIdentifierAttorney
{
  public:
    static const Configuration* GetConfiguration() noexcept
    {
        return InstanceIdentifier::configuration_;
    }
};

namespace
{

// This fixture inherity from SingleTestPerProcessFixture to allow resetting the Runtime Meyer-singleton between tests.
class RuntimeSingleTestPerProcessFixture : public singleton::test::SingleTestPerProcessFixture
{
  public:
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
    std::string config_with_tire_pressure_port_{get_path("mw_com_config.json")};
    InstanceSpecifier tire_pressure_port_other_{
        InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePortOther"}).value()};
    std::string config_with_tire_pressure_port_other_{get_path("mw_com_config_other.json")};
    std::string config_to_merge_{get_path("mw_com_config_to_merge.json")};
    std::string config_to_merge_second_{get_path("mw_com_config_to_merge_second.json")};
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
    filesystem::Path target{"etc/mw_com_config.json"};
    ASSERT_TRUE(filesystem.CopyFile(filesystem::Path{source_path}, target).has_value());
}

void WithNoFileAtDefaultPath()
{
    auto& filesystem = filesystem::IStandardFilesystem::instance();
    filesystem::Path dir{"etc"};
    score::cpp::ignore = filesystem.RemoveAll(dir);
}

using RuntimeInitializationTest = RuntimeSingleTestPerProcessFixture;

TEST_F(RuntimeInitializationTest, ConstructorRegistersItsOwnConfigurationWithInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-18448357, SCR-18448382");
    RecordProperty("Description",
                   "The impl::Runtime constructor registers a pointer to its own "
                   "configuration_ member with InstanceIdentifier::SetConfiguration, so that "
                   "InstanceIdentifier::Create operates on the locked/stable runtime configuration.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([this]() {
        // Given an explicitly initialized runtime
        const auto runtime_configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port_};
        Runtime::Initialize(runtime_configuration);

        // When the Runtime singleton gets constructed (locked)
        auto& runtime = static_cast<Runtime&>(Runtime::getInstance());

        // Then InstanceIdentifier holds a pointer to exactly the runtime's own configuration_ member
        const Configuration* const registered_configuration = InstanceIdentifierAttorney::GetConfiguration();
        ASSERT_NE(registered_configuration, nullptr);
        EXPECT_EQ(registered_configuration, RuntimeAttorney{runtime}.GetConfigurationAddress());
    });
}

TEST_F(RuntimeInitializationTest, ImplicitInitializationAlsoRegistersConfigurationWithInstanceIdentifier)
{
    RecordProperty("Verifies", "SCR-18448357, SCR-18448382");
    RecordProperty("Description",
                   "Also for implicit (default) initialization the Runtime constructor registers its own "
                   "configuration_ member with InstanceIdentifier, enabling InstanceIdentifier::Create.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([this]() {
        // Given a configuration at the default location and implicit default initialization
        WithConfigAtDefaultPath(config_with_tire_pressure_port_);

        // When implicitly constructing the Runtime singleton
        auto& runtime = static_cast<Runtime&>(Runtime::getInstance());

        // Then InstanceIdentifier holds a pointer to exactly the runtime's own configuration_ member
        const Configuration* const registered_configuration = InstanceIdentifierAttorney::GetConfiguration();
        ASSERT_NE(registered_configuration, nullptr);
        EXPECT_EQ(registered_configuration, RuntimeAttorney{runtime}.GetConfigurationAddress());
    });
}

TEST_F(RuntimeInitializationTest, InitializationLoadsCorrectConfiguration)
{
    RecordProperty("Verifies", "SCR-6221480, SCR-21781439");
    RecordProperty("Description", "InstanceSpecifier resolution can not retrieve wrong InstanceIdentifier.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    TestInSeparateProcess([this]() {
        // Given a RuntimeConfiguration
        const auto runtime_configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port_};

        // When initializing the runtime with the call args
        Runtime::Initialize(runtime_configuration);

        // Then we can resolve this instance identifier
        auto identifiers = Runtime::getInstance().resolve(tire_pressure_port_);
        EXPECT_EQ(identifiers.size(), 1);
    });
}

TEST_F(RuntimeInitializationTest, SecondInitializationUpdatesRuntimeIfRuntimeHasNotYetBeenUsed)
{
    TestInSeparateProcess([this]() {
        // Given two RuntimeConfigurations containing different configuration file paths
        const auto runtime_configuration_1 = runtime::RuntimeConfiguration{config_with_tire_pressure_port_};
        const auto runtime_configuration_2 = runtime::RuntimeConfiguration{config_with_tire_pressure_port_other_};

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

TEST_F(RuntimeInitializationTest, SecondInitializationDoesNotUpdateRuntimeIfRuntimeHasAlreadyBeenUsed)
{
    TestInSeparateProcess([this]() {
        // Given two RuntimeConfigurations containing different configuration file paths
        const auto runtime_configuration_1 = runtime::RuntimeConfiguration{config_with_tire_pressure_port_};
        const auto runtime_configuration_2 = runtime::RuntimeConfiguration{config_with_tire_pressure_port_other_};

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

TEST_F(RuntimeInitializationTest, ImplicitInitializationLoadsCorrectConfiguration)
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

TEST_F(RuntimeInitializationTest, AddOnConfigurationInitializationSucceedsIfThereIsNoFileInTheDefaultPath)
{
    TestInSeparateProcess([this]() {
        // Given there is no mw_com_config.json in the default path
        WithNoFileAtDefaultPath();

        const auto configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port_other_};

        // When loading the configuration as an add-on configuration
        const auto init_result = Runtime::InitializeRuntimeAddonConfiguration(configuration);
        auto& runtime = static_cast<Runtime&>(Runtime::getInstance());

        RuntimeAttorney attorney{runtime};
        // Then the configuration should be loaded successfully and the "add-on config" flag should be set
        EXPECT_TRUE(init_result.has_value());
        EXPECT_TRUE(attorney.IsAddonConfigurationLoaded());
    });
}

TEST_F(RuntimeInitializationTest, ConfigurationGetsMergedAndLoadedIfInitialConfigurationIsLocked)
{
    TestInSeparateProcess([this]() {
        // Given there is no mw_com_config.json in the default path
        WithNoFileAtDefaultPath();

        const auto configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port_other_};

        // When loading an initial mw_com_config.json and locking it (via GetInstance()) ...
        Runtime::Initialize(configuration);
        auto& init_runtime = static_cast<Runtime&>(Runtime::getInstance());

        // ... and another add-on configuration afterward
        const auto addon_init_result =
            Runtime::InitializeRuntimeAddonConfiguration(runtime::RuntimeConfiguration{config_to_merge_});

        auto& updated_runtime = static_cast<Runtime&>(Runtime::getInstance());

        const RuntimeAttorney attorney{updated_runtime};

        // Then both configurations are loaded and merged into the runtime's configuration
        EXPECT_TRUE(addon_init_result.has_value());
        EXPECT_EQ(attorney.GetConfigurationAddress()->GetServiceTypes().size(), 2);
        // ... and the "add-on configuration loaded" flag is not set, because it was not loaded as the initial one
        EXPECT_FALSE(attorney.IsAddonConfigurationLoaded());
    });
}

TEST_F(RuntimeInitializationTest, ConfigurationGetsMergedIfInitialConfigurationExists)
{
    TestInSeparateProcess([this]() {
        // Given there is no mw_com_config.json in the default path
        WithNoFileAtDefaultPath();

        const auto configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port_other_};

        // When loading an initial mw_com_config.json...
        Runtime::Initialize(configuration);

        // ... and another add-on configuration afterward and creating the Runtime afterwards
        const auto addon_init_result =
            Runtime::InitializeRuntimeAddonConfiguration(runtime::RuntimeConfiguration{config_to_merge_});

        auto& runtime = static_cast<Runtime&>(Runtime::getInstance());

        const RuntimeAttorney attorney{runtime};

        // Then both configurations are loaded and merged into the runtime's configuration
        EXPECT_TRUE(addon_init_result.has_value());
        EXPECT_EQ(attorney.GetConfigurationAddress()->GetServiceTypes().size(), 2);
        // ... and the "add-on configuration loaded" flag is not set, because it was not loaded as the initial one
        EXPECT_FALSE(attorney.IsAddonConfigurationLoaded());
    });
}

TEST_F(RuntimeInitializationTest, ConcurrentAddonConfigurationInitializationSucceeds)
{
    TestInSeparateProcess([this]() {
        // Given there is no mw_com_config.json in the default path
        WithNoFileAtDefaultPath();

        // And an initial configuration has been loaded
        const auto configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port_other_};
        Runtime::Initialize(configuration);

        // When two threads concurrently add non-conflicting addon configurations
        std::optional<Result<void>> result_thread_1{};
        std::optional<Result<void>> result_thread_2{};

        std::thread thread_1{[&result_thread_1, this]() {
            result_thread_1 =
                Runtime::InitializeRuntimeAddonConfiguration(runtime::RuntimeConfiguration{config_to_merge_});
        }};

        std::thread thread_2{[&result_thread_2, this]() {
            result_thread_2 =
                Runtime::InitializeRuntimeAddonConfiguration(runtime::RuntimeConfiguration{config_to_merge_second_});
        }};

        thread_1.join();
        thread_2.join();

        // Then both addon configurations are successfully merged
        ASSERT_TRUE(result_thread_1.has_value());
        ASSERT_TRUE(result_thread_2.has_value());
        EXPECT_TRUE(result_thread_1->has_value());
        EXPECT_TRUE(result_thread_2->has_value());

        auto& runtime = static_cast<Runtime&>(Runtime::getInstance());
        const RuntimeAttorney attorney{runtime};

        // And all three configurations (initial + 2 addons) are present
        EXPECT_EQ(attorney.GetConfigurationAddress()->GetServiceTypes().size(), 3);
    });
}

using RuntimeInitializationDeathTest = RuntimeInitializationTest;
TEST_F(RuntimeInitializationDeathTest, InitializationFailsIfAddonConfigurationHasBeenLoadedAlready)
{
    // EXPECT_DEATH forks a child process and GTest only allows one stderr capturer at a time
    tested_in_separate_process_ = true;

    EXPECT_DEATH(
        {
            // Given an add-on configuration has been loaded and because it was the first one, it was treated as initial
            // configuration
            const auto runtime_configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port_};
            Runtime::InitializeRuntimeAddonConfiguration(runtime_configuration);
            auto& runtime = static_cast<Runtime&>(Runtime::getInstance());
            // When loading another configuration via Initialize()
            Runtime::Initialize(runtime_configuration);
            // Then the process terminates via std::terminate()
        },
        ".*");
}

TEST_F(RuntimeInitializationDeathTest, AddOnConfigurationInitializationFailsIfThereIsAFileInTheDefaultPath)
{
    // EXPECT_DEATH forks a child process and GTest only allows one stderr capturer at a time
    tested_in_separate_process_ = true;

    EXPECT_DEATH(
        {
            // Given there is a valid mw_com_config.json in the default path
            WithConfigAtDefaultPath(config_with_tire_pressure_port_);
            // When calling InitializeRuntimeAddonConfiguration()
            Runtime::InitializeRuntimeAddonConfiguration(
                runtime::RuntimeConfiguration{config_with_tire_pressure_port_other_});
            // Then the process terminates via std::terminate()
        },
        ".*");
}

TEST_F(RuntimeInitializationDeathTest, AddOnConfigurationInitializationFailsIfMergingTheConfigurationFails)
{
    // EXPECT_DEATH forks a child process and GTest only allows one stderr capturer at a time
    tested_in_separate_process_ = true;

    EXPECT_DEATH(
        {
            WithNoFileAtDefaultPath();
            // Given an add-on configuration has been loaded, and it is being locked by accessing the Runtime instance
            const auto runtime_configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port_};
            Runtime::Initialize(runtime_configuration);
            auto& runtime = static_cast<Runtime&>(Runtime::getInstance());
            // When loading the same configuration via InitializeRuntimeAddonConfiguration()
            const auto add_on_configuration = runtime::RuntimeConfiguration{config_with_tire_pressure_port_};
            Runtime::InitializeRuntimeAddonConfiguration(add_on_configuration);
            // Then the process terminates via std::terminate() because there is a clash of service identifiers
        },
        ".*");
}

using RuntimeTest = RuntimeSingleTestPerProcessFixture;

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
        WithConfigAtDefaultPath(get_path("mw_com_config_invalid_trace_config_path.json"));

        // When implicitly default-initializing the runtime
        score::cpp::ignore = Runtime::getInstance();

        // Then tracing will be disabled
        EXPECT_EQ(Runtime::getInstance().GetTracingRuntime(), nullptr);
    });
}

TEST_F(RuntimeTest, TracingRuntimeIsDisabledWhenTracingDisabledInConfig)
{
    TestInSeparateProcess([]() {
        // Given a configuration with valid and disabled tracing configuration
        WithConfigAtDefaultPath(get_path("mw_com_config_disabled_trace_config.json"));

        // When implicitly default-initializing the runtime
        score::cpp::ignore = Runtime::getInstance();

        // Then tracing will be disabled
        EXPECT_EQ(Runtime::getInstance().GetTracingRuntime(), nullptr);
    });
}

TEST_F(RuntimeTest, TracingRuntimeIsCreatedIfConfiguredCorrectly)
{
    TestInSeparateProcess([]() {
        // Given a configuration with valid and enabled tracing configuration
        auto default_path = get_path("mw_com_config_valid_trace_config.json");
        auto json_path = default_path.find("external") != std::string::npos
                             ? get_path("mw_com_config_valid_trace_config_external.json")
                             : default_path;
        WithConfigAtDefaultPath(json_path);

        // When implicitly default-initializing the runtime
        score::cpp::ignore = Runtime::getInstance();

        // Then tracing runtime will exist
        EXPECT_NE(Runtime::getInstance().GetTracingRuntime(), nullptr);
    });
}

}  // namespace
}  // namespace score::mw::com::impl
