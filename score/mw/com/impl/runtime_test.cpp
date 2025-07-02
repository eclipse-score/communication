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

#include "score/analysis/tracing/common/interface_types/types.h"
#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/runtime_mock.h"
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/configuration/tracing_configuration.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/tracing/configuration/proxy_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config.h"
#include "score/mw/com/impl/tracing/tracing_runtime.h"
#include "score/mw/com/impl/tracing/tracing_test_resources.h"

#include "score/analysis/tracing/library/generic_trace_api/mocks/trace_library_mock.h"

#include <score/optional.hpp>
#include <score/span.hpp>
#include <score/string_view.hpp>

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using testing::_;
using testing::Return;

/**
 * @brief TC verifies, that Runtime::Initialize() fails with abort, when NO "-service_instance_manifest" option is
 * given.
 */
TEST(RuntimeDeathTest, initNoManifestOption)
{
    score::StringLiteral test_args[] = {"dummyname", "arg1", "arg2", "arg3"};
    const score::cpp::span<const score::StringLiteral> test_args_span{test_args};

    // Initialize without mandatory option "-service_instance_manifest"
    EXPECT_DEATH(Runtime::Initialize(test_args_span), ".*");
}

/**
 * @brief TC verifies, that Runtime::Initialize() fails with abort, when "-service_instance_manifest" option is
 * given, but no additional path parameter.
 */
TEST(RuntimeDeathTest, initMissingManifestPath)
{
    score::StringLiteral test_args[] = {"dummyname", "-service_instance_manifest"};
    const score::cpp::span<const score::StringLiteral> test_args_span{test_args};

    // Initialize without arg/path-value for mandatory option "-service_instance_manifest"
    EXPECT_DEATH(Runtime::Initialize(test_args_span), ".*");
}

TEST(RuntimeDeathTest, InvalidJSONTerminates)
{
    EXPECT_DEATH(Runtime::Initialize(std::string{"{"}), ".*");
}

TEST(RuntimeTest, CanRetrieveServiceDiscovery)
{
    Configuration dummy_configuration{Configuration::ServiceTypeDeployments{},
                                      Configuration::ServiceInstanceDeployments{},
                                      GlobalConfiguration{},
                                      TracingConfiguration{}};
    score::cpp::optional<tracing::TracingFilterConfig> empty_filter_configuration{};
    Runtime runtime{std::make_pair(std::move(dummy_configuration), std::move(empty_filter_configuration))};
    score::cpp::ignore = runtime.GetServiceDiscovery();
}

class RuntimeFixture : public ::testing::Test
{
  public:
    RuntimeFixture() noexcept {}

    RuntimeFixture& CreateRuntime() noexcept
    {
        EXPECT_NE(configuration_, nullptr);
        runtime_ =
            std::make_unique<Runtime>(std::make_pair(std::move(*configuration_), std::move(trace_filter_config_)));
        configuration_ = nullptr;

        return *this;
    }

    RuntimeFixture& WithAnEmptyConfiguration() noexcept
    {
        configuration_ = std::make_unique<Configuration>(Configuration::ServiceTypeDeployments{},
                                                         Configuration::ServiceInstanceDeployments{},
                                                         GlobalConfiguration{},
                                                         TracingConfiguration{});
        return *this;
    }

    RuntimeFixture& WithAConfigurationContaining(TracingConfiguration tracing_configuration) noexcept
    {
        configuration_ = std::make_unique<Configuration>(Configuration::ServiceTypeDeployments{},
                                                         Configuration::ServiceInstanceDeployments{},
                                                         GlobalConfiguration{},
                                                         std::move(tracing_configuration));
        return *this;
    }

    RuntimeFixture& WithATracingFilterConfig(tracing::TracingFilterConfig tracing_filter_config) noexcept
    {
        trace_filter_config_ = std::move(tracing_filter_config);
        return *this;
    }

    RuntimeFixture& WithARegisteredServiceTypeDeployment() noexcept
    {
        EXPECT_NE(configuration_, nullptr);
        LolaServiceTypeDeployment service_type_deployment{42};
        configuration_->AddServiceTypeDeployment(make_ServiceIdentifierType("dummyTypeName", 0, 0),
                                                 ServiceTypeDeployment{service_type_deployment});
        return *this;
    }

    bool IsTracingEnabled() const noexcept
    {
        auto* const tracing_runtime = runtime_->GetTracingRuntime();
        EXPECT_NE(tracing_runtime, nullptr);
        return tracing_runtime->IsTracingEnabled();
    }

    analysis::tracing::TraceClientId GetStoredTraceClientId() const noexcept
    {
        auto* const tracing_runtime = dynamic_cast<tracing::TracingRuntime*>(runtime_->GetTracingRuntime());
        EXPECT_NE(tracing_runtime, nullptr);
        tracing::TracingRuntimeAttorney tracing_runtime_attorney{*tracing_runtime};

        const auto tracing_runtime_bindings = tracing_runtime_attorney.GetTracingRuntimeBindings();
        const auto* const lola_tracing_runtime = tracing_runtime_bindings.at(BindingType::kLoLa);
        EXPECT_NE(lola_tracing_runtime, nullptr);

        return lola_tracing_runtime->GetTraceClientId();
    }

    score::cpp::optional<tracing::TracingFilterConfig> trace_filter_config_{};
    std::unique_ptr<Configuration> configuration_{nullptr};
    std::unique_ptr<Runtime> runtime_{nullptr};

    // The created impl::tracing::TracingRuntime will create binding specific TracingRuntimes, which will register
    // itself with the GenericTraceAPI within their ctor. Therefore we need to setup a mock for the GenericTraceAPI.
    std::unique_ptr<score::analysis::tracing::TraceLibraryMock> generic_trace_api_mock_ =
        std::make_unique<score::analysis::tracing::TraceLibraryMock>();
};

TEST_F(RuntimeFixture, CtorWillCreateBindingRuntimes)
{
    // When creating a Runtime object
    WithAnEmptyConfiguration().WithARegisteredServiceTypeDeployment().CreateRuntime();

    // Then a binding runtime will be created
    EXPECT_NE(runtime_->GetBindingRuntime(BindingType::kLoLa), nullptr);
}

TEST_F(RuntimeFixture, CanInjectMock)
{
    // Given a Runtime object with an empty tracing filter config
    WithAnEmptyConfiguration().CreateRuntime();

    RuntimeMock mock_runtime{};
    Runtime::InjectMock(&mock_runtime);

    // When getting the TracingFilterConfig from the runtime
    auto& runtime = Runtime::getInstance();

    // Then the mocked runtime is returned
    EXPECT_EQ(&runtime, &mock_runtime);
}

using RuntimeTracingConfigTest = RuntimeFixture;
TEST_F(RuntimeTracingConfigTest, GetTracingFilterConfigWillReturnEmptyOptionalIfNotSet)
{
    // Given a Runtime object with an empty tracing filter config
    WithAnEmptyConfiguration().CreateRuntime();

    // When getting the TracingFilterConfig from the runtime
    auto tracing_config = runtime_->GetTracingFilterConfig();

    // Then a nullptr is returned
    EXPECT_EQ(tracing_config, nullptr);
}

TEST_F(RuntimeTracingConfigTest, CreatingRuntimeWillRegisterClientIfTracingEnabledAndFilterConfigExists)
{
    RecordProperty("Verifies", "SCR-18159752");
    RecordProperty("Description",
                   "Checks whether Runtime will call RegisterClient if tracing is enabled and a TracingFilterConfig is "
                   "provided. It should be called with the lola binding type and the correct applicationInstanceID.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr const auto* const expected_app_instance_id = "my_application_instance_id";

    // Given a configuration, where tracing is enabled and a TracingFilterConfig is provided
    tracing::TracingFilterConfig tracing_filter_config{};
    TracingConfiguration tracing_configuration{};
    tracing_configuration.SetTracingEnabled(true);
    tracing_configuration.SetApplicationInstanceID(expected_app_instance_id);

    WithAConfigurationContaining(std::move(tracing_configuration))
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config));

    // Expecting that the LoLa specific tracing runtime will call RegisterClient() at the GenericTraceAPI with the lola
    // binding type and the correct applicationInstanceId
    EXPECT_CALL(*generic_trace_api_mock_,
                RegisterClient(analysis::tracing::BindingType::kLoLa, expected_app_instance_id));

    // when we create a Runtime with the configuration and the trace filter configuration.
    CreateRuntime();
}

TEST_F(RuntimeTracingConfigTest, TracingWillBeDisabledIfRegisterClientReturnsAnError)
{
    RecordProperty("Verifies", "SCR-18159752");
    RecordProperty("Description", "Checks whether tracing is disabled if RegisterClient returns an error.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a configuration, where tracing is enabled and a TracingFilterConfig is provided
    tracing::TracingFilterConfig tracing_filter_config{};
    TracingConfiguration tracing_configuration{};
    tracing_configuration.SetTracingEnabled(true);

    WithAConfigurationContaining(std::move(tracing_configuration))
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config));

    // Expecting that the LoLa specific tracing runtime will call RegisterClient() at the GenericTraceAPI which returns
    // an error
    EXPECT_CALL(*generic_trace_api_mock_, RegisterClient(_, _))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kNotEnoughMemoryRecoverable)));

    // when we create a Runtime with the configuration and the trace filter configuration.
    CreateRuntime();

    // Then tracing will be disabled
    EXPECT_FALSE(IsTracingEnabled());
}

TEST_F(RuntimeTracingConfigTest, TraceClientIdWillBeSavedWhenRegisterClientSucceeds)
{
    RecordProperty("Verifies", "SCR-18172251");
    RecordProperty("Description", "Checks whether the TraceClientId returned by RegisterClient will be saved.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const analysis::tracing::TraceClientId trace_client_id{42};

    // Given a configuration, where tracing is enabled and a TracingFilterConfig is provided
    tracing::TracingFilterConfig tracing_filter_config{};
    TracingConfiguration tracing_configuration{};
    tracing_configuration.SetTracingEnabled(true);

    WithAConfigurationContaining(std::move(tracing_configuration))
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config));

    // Expecting that the LoLa specific tracing runtime will call RegisterClient() at the GenericTraceAPI
    ON_CALL(*generic_trace_api_mock_, RegisterClient(_, _)).WillByDefault(Return(trace_client_id));

    // when we create a Runtime with the configuration and the trace filter configuration.
    CreateRuntime();

    // Then the TraceClientId will be saved
    EXPECT_EQ(GetStoredTraceClientId(), trace_client_id);
}

TEST_F(RuntimeTracingConfigTest,
       CreatingRuntimeWillRegisterTraceDoneCBWithClientIdFromClientRegistrationIfRegisterClientSucceeded)
{
    RecordProperty("Verifies", "SCR-18194091");
    RecordProperty(
        "Description",
        "Checks whether RegisterTraceDoneCB is called with the correct TraceClientId if RegisterClient succeeds.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const analysis::tracing::TraceClientId trace_client_id{42};

    // Given a configuration, where tracing is enabled and a TracingFilterConfig is provided
    tracing::TracingFilterConfig tracing_filter_config{};
    TracingConfiguration tracing_configuration{};
    tracing_configuration.SetTracingEnabled(true);

    WithAConfigurationContaining(std::move(tracing_configuration))
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config));

    // Expecting that the LoLa specific tracing runtime in its ctor will call RegisterClient() at the GenericTraceAPI
    ON_CALL(*generic_trace_api_mock_, RegisterClient(_, _)).WillByDefault(Return(trace_client_id));

    // and will register a trace-done-callback at the GenericTraceAPI with the trace client id returned by
    // RegisterClient
    EXPECT_CALL(*generic_trace_api_mock_, RegisterTraceDoneCB(trace_client_id, _));

    // when we create a Runtime with the configuration and the trace filter configuration.
    CreateRuntime();
}

TEST_F(RuntimeTracingConfigTest, RegisterTraceDoneCBWillNotBeCalledIfRegisterClientReturnsAnError)
{
    RecordProperty("Verifies", "SCR-18194091");
    RecordProperty("Description", "Checks that RegisterTraceDoneCB is not called if RegisterClient returns an error.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const analysis::tracing::TraceClientId trace_client_id{42};

    // Given a configuration, where tracing is enabled and a TracingFilterConfig is provided
    tracing::TracingFilterConfig tracing_filter_config{};
    TracingConfiguration tracing_configuration{};
    tracing_configuration.SetTracingEnabled(true);

    WithAConfigurationContaining(std::move(tracing_configuration))
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config));

    // Expecting that the LoLa specific tracing runtime in its ctor will call RegisterClient() at the GenericTraceAPI
    ON_CALL(*generic_trace_api_mock_, RegisterClient(_, _))
        .WillByDefault(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kInvalidAppInstanceIdFatal)));

    // and will _not_ register a trace-done-callback at the GenericTraceAPI
    EXPECT_CALL(*generic_trace_api_mock_, RegisterTraceDoneCB(trace_client_id, _)).Times(0);

    // when we create a Runtime with the configuration and the trace filter configuration.
    CreateRuntime();
}

TEST_F(RuntimeTracingConfigTest, TracingWillBeDisabledIfRegisterTraceDoneCBReturnsAnError)
{
    RecordProperty("Verifies", "SCR-18194091");
    RecordProperty("Description", "Checks that tracing is disabled if RegisterTraceDoneCB returns an error.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const analysis::tracing::TraceClientId trace_client_id{42};

    // Given a configuration, where tracing is enabled and a TracingFilterConfig is provided
    tracing::TracingFilterConfig tracing_filter_config{};
    TracingConfiguration tracing_configuration{};
    tracing_configuration.SetTracingEnabled(true);

    WithAConfigurationContaining(std::move(tracing_configuration))
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config));

    // Expecting that the LoLa specific tracing runtime in its ctor will call RegisterClient() at the GenericTraceAPI
    ON_CALL(*generic_trace_api_mock_, RegisterClient(_, _)).WillByDefault(Return(trace_client_id));

    // and expecting that RegisterTraceDoneCB will return an error
    EXPECT_CALL(*generic_trace_api_mock_, RegisterTraceDoneCB(_, _))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kCallbackAlreadyRegisteredRecoverable)));

    // when we create a Runtime with the configuration and the trace filter configuration.
    CreateRuntime();

    // Then tracing will be disabled
    EXPECT_FALSE(IsTracingEnabled());
}

TEST_F(RuntimeTracingConfigTest, CreatingRuntimeWillCreateTracingRuntimeIfTracingEnabledAndFilterConfigExists)
{
    RecordProperty("Verifies", "SCR-18159733");
    RecordProperty("Description",
                   "Checks that IPC tracing runtime will be created if tracing is enabled in TracingConfiguration and "
                   "a valid Trace Filter config is provided.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a configuration, where tracing is enabled and a TracingFilterConfig is provided
    tracing::TracingFilterConfig tracing_filter_config{};
    TracingConfiguration tracing_configuration{};
    tracing_configuration.SetTracingEnabled(true);

    WithAConfigurationContaining(std::move(tracing_configuration))
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config));

    // when we create a Runtime with the configuration and the trace filter configuration.
    CreateRuntime();

    // Then the runtime will contain a valid tracing runtime
    EXPECT_NE(runtime_->GetTracingRuntime(), nullptr);

    // and a LoLa specific runtime and a LoLa specific tracing runtime
    ASSERT_NE(runtime_->GetBindingRuntime(BindingType::kLoLa), nullptr);
    EXPECT_NE(runtime_->GetBindingRuntime(BindingType::kLoLa)->GetTracingRuntime(), nullptr);
}

TEST_F(RuntimeTracingConfigTest, CreatingRuntimeNotCreateTracingRuntimeIfTracingDisabled)
{
    RecordProperty("Verifies", "SCR-18159733");
    RecordProperty(
        "Description",
        "Checks that IPC tracing runtime will not be created if tracing is disabled in TracingConfiguration.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a configuration, where tracing is disabled and a TracingFilterConfig is provided
    tracing::TracingFilterConfig tracing_filter_config{};
    TracingConfiguration tracing_configuration{};
    tracing_configuration.SetTracingEnabled(false);

    WithAConfigurationContaining(std::move(tracing_configuration))
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config));

    // when we create a Runtime with the configuration and the trace filter configuration.
    CreateRuntime();

    // Then the runtime will not contain a valid tracing runtime
    EXPECT_EQ(runtime_->GetTracingRuntime(), nullptr);

    // and will contain a valid LoLa specific runtime, but NO Lola specific tracing runtime.
    ASSERT_NE(runtime_->GetBindingRuntime(BindingType::kLoLa), nullptr);
    EXPECT_EQ(runtime_->GetBindingRuntime(BindingType::kLoLa)->GetTracingRuntime(), nullptr);
}

TEST_F(RuntimeTracingConfigTest, CreatingRuntimeNotCreateTracingRuntimeIfNoTraceFilterConfigExists)
{
    RecordProperty("Verifies", "SCR-18159733");
    RecordProperty(
        "Description",
        "Checks that IPC tracing runtime will not be created if a valid Trace Filter config is not provided.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a configuration, where tracing is enabled and a TracingFilterConfig is _not_ provided
    TracingConfiguration tracing_configuration{};
    tracing_configuration.SetTracingEnabled(true);

    WithAConfigurationContaining(std::move(tracing_configuration)).WithARegisteredServiceTypeDeployment();

    // when we create a Runtime with the configuration and the trace filter configuration.
    CreateRuntime();

    // Then the runtime will not contain a valid tracing runtime
    EXPECT_EQ(runtime_->GetTracingRuntime(), nullptr);

    // and will contain a valid LoLa specific runtime, but NO Lola specific tracing runtime.
    ASSERT_NE(runtime_->GetBindingRuntime(BindingType::kLoLa), nullptr);
    EXPECT_EQ(runtime_->GetBindingRuntime(BindingType::kLoLa)->GetTracingRuntime(), nullptr);
}

TEST_F(RuntimeTracingConfigTest, GetTracingFilterConfigWillReturnConfigPassedToConstructor)
{
    // Given a configuration, where tracing is enabled and a TracingFilterConfig is provided
    tracing::TracingFilterConfig tracing_filter_config{};
    WithAnEmptyConfiguration()
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config))
        .CreateRuntime();

    // When getting the TracingFilterConfig from the runtime
    const auto* const output_tracing_filter_config = runtime_->GetTracingFilterConfig();

    // Then the TracingFilterConfig will return a TracingFilterConfig
    EXPECT_NE(output_tracing_filter_config, nullptr);
}

TEST_F(RuntimeTracingConfigTest, TracingFilterConfigRetrievedFromRuntimeWillHaveSameTracePointedEnabled)
{
    const score::cpp::string_view service_type_0{"service_type_0"};
    const score::cpp::string_view service_type_1{"service_type_1"};
    const score::cpp::string_view event_name_0{"event_name_0"};
    const score::cpp::string_view event_name_1{"event_name_1"};
    const score::cpp::string_view instance_specifier_view_0{"instance_specifier_view_0"};
    const score::cpp::string_view instance_specifier_view_1{"instance_specifier_view_1"};
    const auto trace_point_0 = tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE;
    const auto trace_point_1 = tracing::ProxyEventTracePointType::GET_NEW_SAMPLES;

    // Given a configuration, where tracing is enabled and a TracingFilterConfig is provided with some trace points
    // enabled
    tracing::TracingFilterConfig tracing_filter_config{};
    tracing_filter_config.AddTracePoint(service_type_0, event_name_0, instance_specifier_view_0, trace_point_0);
    tracing_filter_config.AddTracePoint(service_type_1, event_name_1, instance_specifier_view_1, trace_point_1);

    WithAnEmptyConfiguration()
        .WithARegisteredServiceTypeDeployment()
        .WithATracingFilterConfig(std::move(tracing_filter_config))
        .CreateRuntime();

    // When enabling trace points in the input TracingFilterConfig and creating a Runtime
    const auto* const output_tracing_filter_config = runtime_->GetTracingFilterConfig();
    ASSERT_NE(output_tracing_filter_config, nullptr);

    // Then the TracingFilterConfig retrieved from the runtime has the same trace points enabled
    const bool is_trace_point_enabled_0 = output_tracing_filter_config->IsTracePointEnabled(
        service_type_0, event_name_0, instance_specifier_view_0, trace_point_0);
    const bool is_trace_point_enabled_1 = output_tracing_filter_config->IsTracePointEnabled(
        service_type_1, event_name_1, instance_specifier_view_1, trace_point_1);

    EXPECT_TRUE(is_trace_point_enabled_0);
    EXPECT_TRUE(is_trace_point_enabled_1);
}

}  // namespace
}  // namespace score::mw::com::impl
