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
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_event.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/tracing/common_event_tracing.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_field_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config_mock.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"
#include "score/mw/com/impl/tracing/test/runtime_mock_guard.h"
#include "score/mw/com/impl/tracing/trace_error.h"
#include "score/mw/com/impl/tracing/tracing_runtime_mock.h"

#include <score/assert.hpp>

#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace score::mw::com::impl::tracing
{
namespace
{

using namespace ::testing;

const std::string kDummyEventName{"my_dummy_event"};
const std::string kDummyFieldName{"my_dummy_field"};

const LolaServiceInstanceDeployment kLolaServiceInstanceDeploymentWithEventAndField{
    LolaServiceInstanceId{1U},
    {{kDummyEventName, LolaEventInstanceDeployment{10U, 10U, 2U, true, 0}}},
    {{kDummyFieldName, LolaFieldInstanceDeployment{10U, 10U, 2U, true, 0}}}

};
const LolaServiceTypeDeployment kLolaServiceTypeDeploymentWithEventAndField{2U,
                                                                            {{kDummyEventName, LolaEventId{3U}}},
                                                                            {{kDummyFieldName, LolaEventId{4U}}}};

const auto kInstanceSpecifier = InstanceSpecifier::Create("/bla/blub/specifier").value();
const ConfigurationStore kConfigStore{kInstanceSpecifier,
                                      make_ServiceIdentifierType("foo"),
                                      QualityType::kASIL_QM,
                                      kLolaServiceTypeDeploymentWithEventAndField,
                                      kLolaServiceInstanceDeploymentWithEventAndField};

constexpr ServiceElementTracingData kDummyServiceElementTracingData{2U, 5U};

bool AreAllTracePointsDisabled(const SkeletonEventTracingData& skeleton_event_tracing_data)
{
    return (!skeleton_event_tracing_data.enable_send && !skeleton_event_tracing_data.enable_send_with_allocate);
}

class SkeletonEventTracingFixture : public ::testing::TestWithParam<ServiceElementType>
{
  public:
    SkeletonEventTracingFixture()
    {
        ON_CALL(skeleton_event_binding_base_, GetBindingType()).WillByDefault(Return(BindingType::kFake));
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(&tracing_runtime_mock_));
    }

    SkeletonEventTracingFixture& WithASkeletonEventTracingDataWithInvalidElementType()
    {
        skeleton_event_tracing_data_.service_element_instance_identifier_view.service_element_identifier_view
            .service_element_type = static_cast<ServiceElementType>(100U);
        return *this;
    }

    SkeletonEventTracingFixture& WithAValidSkeletonEventTracingData()
    {
        skeleton_event_tracing_data_.service_element_instance_identifier_view.service_element_identifier_view
            .service_element_type = GetParam();
        return *this;
    }

    SkeletonEventTracingFixture& WithAllTracePointsEnabled()
    {
        skeleton_event_tracing_data_.enable_send = true;
        skeleton_event_tracing_data_.enable_send_with_allocate = true;

        return *this;
    }

    using TestSampleType = std::uint32_t;

    SkeletonEventTracingData skeleton_event_tracing_data_{};
    mock_binding::SkeletonEvent<TestSampleType> skeleton_event_binding_base_{};
    impl::SampleAllocateePtr<TestSampleType> sample_data_ptr_{
        MakeSampleAllocateePtr(std::make_unique<TestSampleType>(10U))};

    TracingRuntimeMock tracing_runtime_mock_{};
    RuntimeMockGuard runtime_mock_guard_{};
};

using SkeletonEventTraceSendFixture = SkeletonEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(SkeletonEventTraceSendFixture,
                         SkeletonEventTraceSendFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using SkeletonEventTraceSendParamaterisedDeathTest = SkeletonEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(SkeletonEventTraceSendParamaterisedDeathTest,
                         SkeletonEventTraceSendParamaterisedDeathTest,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using SkeletonEventTraceSendWithAllocateFixture = SkeletonEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(SkeletonEventTraceSendWithAllocateFixture,
                         SkeletonEventTraceSendWithAllocateFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using SkeletonEventTraceSendWithAllocateParamaterisedDeathTest = SkeletonEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(SkeletonEventTraceSendWithAllocateParamaterisedDeathTest,
                         SkeletonEventTraceSendWithAllocateParamaterisedDeathTest,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(SkeletonEventTraceSendFixture, TraceSendWillDispatchToTracingRuntimeBinding)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // Expecting TraceData will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _));

    // When calling TraceSend
    TraceSend<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);
}

TEST_P(SkeletonEventTraceSendFixture, TraceSendWillNotDispatchToTracingRuntimeBindingIfTracingDisabled)
{
    // Given a SkeletonEventTracingData with all trace points disabled
    WithAValidSkeletonEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceSend
    TraceSend<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);
}

TEST_P(SkeletonEventTraceSendFixture,
       TraceSendWillDisableTracePointIfDisableInstanceErrorReturnedFromTracingRuntimeBinding)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // Expecting TraceData will be called on the TracingRuntime binding which returns a disable trace point instance
    // error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceSend
    TraceSend<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);

    // Then the enable_send trace point is disabled
    EXPECT_FALSE(skeleton_event_tracing_data_.enable_send);
}

TEST_P(SkeletonEventTraceSendFixture,
       TraceSendWillDisableTracePointIfDisableAllTracePointsErrorReturnedFromTracingRuntimeBinding)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // Expecting TraceData will be called on the TracingRuntime binding which returns a disable all trace points error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceSend
    TraceSend<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(skeleton_event_tracing_data_));
}

TEST_P(SkeletonEventTraceSendFixture, TraceSendWillIgnoreUnknownErrorFromTracingRuntimeBinding)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // Expecting TraceData will be called on the TracingRuntime binding which returns an unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceSend
    TraceSend<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);

    // Then the enable_send trace point is still enabled
    EXPECT_TRUE(skeleton_event_tracing_data_.enable_send);
}

TEST_F(SkeletonEventTraceSendParamaterisedDeathTest, TraceSendWithInvalidTraceServiceElementTypeTerminates)
{
    // Given a SkeletonEventTracingData with an invalid element type and all trace points enabled
    WithASkeletonEventTracingDataWithInvalidElementType().WithAllTracePointsEnabled();

    // When calling TraceSend
    // Then we terminate
    EXPECT_DEATH(
        TraceSend<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_), ".*");
}

TEST_P(SkeletonEventTraceSendParamaterisedDeathTest, TraceSendWithAllocateWithNullSampleAllocateePtrTerminates)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // When calling TraceSend with a sample allocatee ptr containing nullptr
    SampleAllocateePtr<TestSampleType> null_sample_allocatee_ptr{nullptr};
    // Then we terminate
    EXPECT_DEATH(TraceSend<TestSampleType>(
                     skeleton_event_tracing_data_, skeleton_event_binding_base_, null_sample_allocatee_ptr),
                 ".*");
}

using SkeletonEventTraceSendWithAllocateFixture = SkeletonEventTracingFixture;
TEST_P(SkeletonEventTraceSendWithAllocateFixture, TraceSendWithAllocateWillDispatchToTracingRuntimeBinding)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _));

    // When calling TraceSendWithAllocate
    TraceSendWithAllocate<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);
}

TEST_P(SkeletonEventTraceSendWithAllocateFixture,
       TraceSendWithAllocateWillNotDispatchToTracingRuntimeBindingIfTracingDisabled)
{
    // Given a SkeletonEventTracingData with all trace points disabled
    WithAValidSkeletonEventTracingData();

    // Expecting Trace will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceSendWithAllocate
    TraceSendWithAllocate<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);
}

TEST_P(SkeletonEventTraceSendWithAllocateFixture,
       TraceSendWithAllocateWillDisableTracePointIfDisableInstanceErrorReturnedFromTracingRuntimeBinding)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // Expecting TraceData will be called on the TracingRuntime binding which returns a disable trace point instance
    // error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceSendWithAllocate
    TraceSendWithAllocate<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);

    // Then the enable_send_with_allocate trace point is disabled
    EXPECT_FALSE(skeleton_event_tracing_data_.enable_send_with_allocate);
}

TEST_P(SkeletonEventTraceSendWithAllocateFixture,
       TraceSendWithAllocateWillDisableTracePointIfDisableAllTracePointsErrorReturnedFromTracingRuntimeBinding)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // Expecting TraceData will be called on the TracingRuntime binding which returns a disable all trace points error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceSendWithAllocate
    TraceSendWithAllocate<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(skeleton_event_tracing_data_));
}

TEST_P(SkeletonEventTraceSendWithAllocateFixture, TraceSendWithAllocateWillIgnoreUnknownErrorFromTracingRuntimeBinding)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // Expecting TraceData will be called on the TracingRuntime binding which returns an unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceSendWithAllocate
    TraceSendWithAllocate<TestSampleType>(skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_);

    // Then the enable_send_with_allocate trace point is still enabled
    EXPECT_TRUE(skeleton_event_tracing_data_.enable_send_with_allocate);
}

TEST_P(SkeletonEventTraceSendWithAllocateParamaterisedDeathTest,
       TraceSendWithAllocateWithInvalidTraceServiceElementTypeTerminates)
{
    // Given a SkeletonEventTracingData with an invalid element type and all trace points enabled
    WithASkeletonEventTracingDataWithInvalidElementType().WithAllTracePointsEnabled();

    // When calling TraceSendWithAllocate
    // Then we terminate
    EXPECT_DEATH(TraceSendWithAllocate<TestSampleType>(
                     skeleton_event_tracing_data_, skeleton_event_binding_base_, sample_data_ptr_),
                 ".*");
}

TEST_P(SkeletonEventTraceSendWithAllocateParamaterisedDeathTest,
       TraceSendWithAllocateWithNullSampleAllocateePtrTerminates)
{
    // Given a SkeletonEventTracingData with all trace points enabled
    WithAValidSkeletonEventTracingData().WithAllTracePointsEnabled();

    // When calling TraceSendWithAllocate with a sample allocatee ptr containing nullptr
    SampleAllocateePtr<TestSampleType> null_sample_allocatee_ptr{nullptr};
    // Then we terminate
    EXPECT_DEATH(TraceSendWithAllocate<TestSampleType>(
                     skeleton_event_tracing_data_, skeleton_event_binding_base_, null_sample_allocatee_ptr),
                 ".*");
}

class SkeletonEventTracingGenerateTracingStructFixture : public ::testing::TestWithParam<ServiceElementType>
{
  public:
    SkeletonEventTracingGenerateTracingStructFixture()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG((GetParam() == ServiceElementType::EVENT) ||
                                            (GetParam() == ServiceElementType::FIELD));

        ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
            .WillByDefault(Return(&tracing_filter_config_mock_));
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(&tracing_runtime_mock_));
    }

    SkeletonEventTracingGenerateTracingStructFixture& WithSendTracePointEnabled(bool is_enabled)
    {
        if (GetParam() == ServiceElementType::EVENT)
        {
            ON_CALL(tracing_filter_config_mock_, IsTracePointEnabled(_, _, _, SkeletonEventTracePointType::SEND))
                .WillByDefault(Return(is_enabled));
        }
        else
        {
            ON_CALL(tracing_filter_config_mock_, IsTracePointEnabled(_, _, _, SkeletonFieldTracePointType::UPDATE))
                .WillByDefault(Return(is_enabled));
        }

        return *this;
    }

    SkeletonEventTracingGenerateTracingStructFixture& WithSendWithAllocateTracePointEnabled(bool is_enabled)
    {
        if (GetParam() == ServiceElementType::EVENT)
        {
            ON_CALL(tracing_filter_config_mock_,
                    IsTracePointEnabled(_, _, _, SkeletonEventTracePointType::SEND_WITH_ALLOCATE))
                .WillByDefault(Return(is_enabled));
        }
        else
        {
            ON_CALL(tracing_filter_config_mock_,
                    IsTracePointEnabled(_, _, _, SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE))
                .WillByDefault(Return(is_enabled));
        }

        return *this;
    }

    SkeletonEventTracingData GenerateSkeletonTracingStruct(const InstanceIdentifier& instance_identifier,
                                                           const BindingType binding_type,
                                                           const std::string_view service_element_name) noexcept
    {
        if (GetParam() == ServiceElementType::EVENT)
        {
            return GenerateSkeletonTracingStructFromEventConfig(
                instance_identifier, binding_type, service_element_name);
        }
        else
        {
            return GenerateSkeletonTracingStructFromFieldConfig(
                instance_identifier, binding_type, service_element_name);
        }
    }

    std::string_view GetServiceElementName()
    {
        if (GetParam() == ServiceElementType::EVENT)
        {
            return kDummyEventName;
        }
        else
        {
            return kDummyFieldName;
        }
    }

    TracingFilterConfigMock tracing_filter_config_mock_{};
    TracingRuntimeMock tracing_runtime_mock_{};
    RuntimeMockGuard runtime_mock_guard_{};
};

INSTANTIATE_TEST_CASE_P(SkeletonEventTracingGenerateTracingStructFixture,
                        SkeletonEventTracingGenerateTracingStructFixture,
                        ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using SkeletonEventTracingGenerateTracingStructDeathTest = SkeletonEventTracingGenerateTracingStructFixture;
INSTANTIATE_TEST_CASE_P(SkeletonEventTracingGenerateTracingStructDeathTest,
                        SkeletonEventTracingGenerateTracingStructDeathTest,
                        ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(SkeletonEventTracingGenerateTracingStructFixture,
       CallingGenerateTracingStructRegistersServiceElementIfATracePointIsEnabled)
{
    // Given a valid tracing runtime and TracingFilterConfig and a trace point enabled in the TracingFilterConfig
    WithSendTracePointEnabled(true);

    // Expecting that RegisterServiceElement is called on the tracing runtime binding
    EXPECT_CALL(tracing_runtime_mock_, RegisterServiceElement(_, _));

    // When calling GenerateSkeletonTracingStructFromEventConfig / GenerateSkeletonTracingStructFromFieldConfig
    score::cpp::ignore = GenerateSkeletonTracingStruct(
        kConfigStore.GetInstanceIdentifier(), BindingType::kFake, GetServiceElementName());
}

TEST_P(SkeletonEventTracingGenerateTracingStructFixture,
       CallingGenerateTracingStructReturnsStructWithServiceElementTracingDataFromRegisterServiceElementCall)
{
    // Given a valid tracing runtime and TracingFilterConfig and a trace point enabled in the TracingFilterConfig
    WithSendTracePointEnabled(true);

    // and that RegisterServiceElement is called on the tracing runtime binding which returns a
    // ServiceElementTracingData
    ON_CALL(tracing_runtime_mock_, RegisterServiceElement(_, _)).WillByDefault(Return(kDummyServiceElementTracingData));

    // When calling GenerateSkeletonTracingStructFromEventConfig / GenerateSkeletonTracingStructFromFieldConfig
    const auto tracing_data = GenerateSkeletonTracingStruct(
        kConfigStore.GetInstanceIdentifier(), BindingType::kFake, GetServiceElementName());

    // Then the provided struct contains the ServiceElementInstanceIdentifierView
    EXPECT_EQ(tracing_data.service_element_tracing_data, kDummyServiceElementTracingData);
}

TEST_P(SkeletonEventTracingGenerateTracingStructFixture,
       CallingGenerateTracingStructReturnsStructWithServiceElementIdentifierFromProvidedInstanceIdentifier)
{
    // Given a valid tracing runtime and TracingFilterConfig and a trace point enabled in the TracingFilterConfig
    WithSendTracePointEnabled(true);

    // When calling GenerateSkeletonTracingStructFromEventConfig / GenerateSkeletonTracingStructFromFieldConfig
    const auto tracing_data = GenerateSkeletonTracingStruct(
        kConfigStore.GetInstanceIdentifier(), BindingType::kFake, GetServiceElementName());

    // Then the provided struct contains the ServiceElementInstanceIdentifierView dervied from the provided instance
    // identifier
    const auto service_element_type = GetParam();
    const auto expected_service_element_instance_identifier_view = GetServiceElementInstanceIdentifierView(
        kConfigStore.GetInstanceIdentifier(), GetServiceElementName(), service_element_type);
    EXPECT_EQ(tracing_data.service_element_instance_identifier_view, expected_service_element_instance_identifier_view);
}

TEST_P(SkeletonEventTracingGenerateTracingStructFixture,
       CallingGenerateTracingStructReturnsStructWithTracePointsEnabledThatAreSetInTraceFilterConfig)
{
    // Given a valid tracing runtime and TracingFilterConfig

    // and that the trace point send is enabled but send_with_allocate is disabled
    WithSendTracePointEnabled(true);
    WithSendWithAllocateTracePointEnabled(false);

    // When calling GenerateSkeletonTracingStructFromEventConfig / GenerateSkeletonTracingStructFromFieldConfig
    const auto tracing_data = GenerateSkeletonTracingStruct(
        kConfigStore.GetInstanceIdentifier(), BindingType::kFake, GetServiceElementName());

    // Then the provided struct should have the send trace point enabled and send_with_allocate disabled
    EXPECT_TRUE(tracing_data.enable_send);
    EXPECT_FALSE(tracing_data.enable_send_with_allocate);
}

TEST_P(SkeletonEventTracingGenerateTracingStructFixture,
       CallingGenerateTracingStructWithNoTracePointsEnabledInTraceFilterConfigReturnsStructWithNoTracePointsEnabled)
{
    // Given a valid tracing runtime and TracingFilterConfig

    // and that all trace points are disabled
    WithSendTracePointEnabled(false);
    WithSendWithAllocateTracePointEnabled(false);

    // When calling GenerateSkeletonTracingStructFromEventConfig / GenerateSkeletonTracingStructFromFieldConfig
    const auto tracing_data = GenerateSkeletonTracingStruct(
        kConfigStore.GetInstanceIdentifier(), BindingType::kFake, GetServiceElementName());

    // Then the provided struct should have all trace points disabled
    EXPECT_FALSE(tracing_data.enable_send);
    EXPECT_FALSE(tracing_data.enable_send_with_allocate);
}

TEST_P(SkeletonEventTracingGenerateTracingStructFixture,
       CallingGenerateTracingStructWithoutTracingFilterConfigReturnsEmptyStruct)
{
    // Given a valid tracing runtime but no TracingFilterConfig
    ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));

    // When calling GenerateSkeletonTracingStructFromEventConfig / GenerateSkeletonTracingStructFromFieldConfig
    const auto tracing_data = GenerateSkeletonTracingStruct(
        kConfigStore.GetInstanceIdentifier(), BindingType::kFake, GetServiceElementName());

    // Then the provided struct should be empty
    EXPECT_EQ(tracing_data, SkeletonEventTracingData{});
}

TEST_P(SkeletonEventTracingGenerateTracingStructFixture, CallingGenerateTracingStructWithoutTracingRuntimeReturnsEmpty)
{
    // Given a valid TracingFilterConfig but no tracing runtime
    ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(nullptr));

    // When calling GenerateSkeletonTracingStructFromEventConfig / GenerateSkeletonTracingStructFromFieldConfig
    const auto tracing_data = GenerateSkeletonTracingStruct(
        kConfigStore.GetInstanceIdentifier(), BindingType::kFake, GetServiceElementName());

    // Then the provided struct should be empty
    EXPECT_EQ(tracing_data, SkeletonEventTracingData{});
}

TEST_P(SkeletonEventTracingGenerateTracingStructDeathTest,
       CallingGenerateTracingStructWithBlankServiceInstanceDeploymentBindingTerminates)
{
    const ServiceInstanceDeployment service_instance_deployment_with_blank_binding{
        make_ServiceIdentifierType("foo"), score::cpp::blank{}, QualityType::kASIL_QM, kInstanceSpecifier};
    const ServiceTypeDeployment service_type_deployment_with_blank_binding{score::cpp::blank{}};
    const auto instance_identifier_with_blank_binding = make_InstanceIdentifier(
        service_instance_deployment_with_blank_binding, service_type_deployment_with_blank_binding);

    // Given a valid tracing runtime and TracingFilterConfig and a trace point enabled in the TracingFilterConfig
    WithSendTracePointEnabled(true);

    // When calling GenerateSkeletonTracingStructFromEventConfig / GenerateSkeletonTracingStructFromFieldConfig with an
    // InstanceIdentifier that contains a blank bindings Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = GenerateSkeletonTracingStruct(
                     instance_identifier_with_blank_binding, BindingType::kFake, GetServiceElementName()),
                 ".*");
}

TEST_P(SkeletonEventTracingGenerateTracingStructDeathTest,
       CallingGenerateTracingStructWithInvalidServiceElementNameTerminates)
{
    // Given a valid tracing runtime and TracingFilterConfig and a trace point enabled in the TracingFilterConfig
    WithSendTracePointEnabled(true);

    // When calling GenerateSkeletonTracingStructFromEventConfig / GenerateSkeletonTracingStructFromFieldConfig with a
    // service element name which doesn't exist in the configuration
    EXPECT_DEATH(score::cpp::ignore = GenerateSkeletonTracingStruct(
                     kConfigStore.GetInstanceIdentifier(), BindingType::kFake, "invalid_event_name"),
                 ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
