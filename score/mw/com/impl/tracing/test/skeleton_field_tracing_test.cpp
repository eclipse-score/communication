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
#include "score/mw/com/impl/skeleton_field.h"

#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_event.h"
#include "score/mw/com/impl/skeleton_event.h"
#include "score/mw/com/impl/skeleton_field.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config_mock.h"
#include "score/mw/com/impl/tracing/test/runtime_mock_guard.h"
#include "score/mw/com/impl/tracing/trace_error.h"
#include "score/mw/com/impl/tracing/tracing_runtime_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace score
{
namespace mw
{
namespace com
{
namespace impl
{
namespace
{

using ::testing::_;
using ::testing::An;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

using TestSampleType = std::uint8_t;

using SkeletonEventTracingData = tracing::SkeletonEventTracingData;

const tracing::ITracingRuntimeBinding::TraceContextId kTraceContextId{0U};

const auto kFieldName{"Field1"};

const auto kInstanceSpecifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
const std::string kServiceTypeName{"foo"};
const auto kServiceIdentifier = make_ServiceIdentifierType(kServiceTypeName, 13, 37);
std::uint16_t kInstanceId{23U};
const ServiceInstanceDeployment kInstanceDeployment{kServiceIdentifier,
                                                    LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
                                                    QualityType::kASIL_QM,
                                                    kInstanceSpecifier};
std::uint16_t kServiceId{34U};
const ServiceTypeDeployment kTypeDeployment{LolaServiceTypeDeployment{kServiceId}};
const auto kInstanceIdentifier = make_InstanceIdentifier(kInstanceDeployment, kTypeDeployment);

class MyDummySkeleton : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;

    SkeletonField<TestSampleType> my_dummy_field_{*this, kFieldName};
};

TEST(SkeletonFieldTracingTest, TracePointsAreDisabledIfConfigNotReturnedByRuntime)
{
    tracing::RuntimeMockGuard runtime_mock_guard{};

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdentifier, _, score::cpp::string_view{kFieldName}))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));
    EXPECT_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillOnce(Return(nullptr));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdentifier};

    // Then all the trace points of the SkeletonEvent should be set according to the calls to IsTracePointEnabled
    const auto actual_enabled_trace_points = SkeletonFieldBaseView{unit.my_dummy_field_}.GetSkeletonEventTracing();
    EXPECT_EQ(actual_enabled_trace_points.service_element_instance_identifier_view,
              tracing::ServiceElementInstanceIdentifierView{});
    EXPECT_EQ(actual_enabled_trace_points.enable_send, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_send_with_allocate, false);
}

class SkeletonFieldTracingParamaterisedFixture : public ::testing::TestWithParam<SkeletonEventTracingData>
{
};

TEST_P(SkeletonFieldTracingParamaterisedFixture, TracePointsAreCorrectlySet)
{
    const SkeletonEventTracingData expected_enabled_trace_points = GetParam();

    tracing::RuntimeMockGuard runtime_mock_guard{};
    tracing::TracingFilterConfigMock tracing_mock{};
    tracing::TracingRuntimeMock tracing_runtime_mock{};

    const auto service_type = kServiceIdentifier.ToString();

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard{};

    const tracing::ServiceElementIdentifierView service_element_identifier_view{
        service_type, kFieldName, ServiceElementType::FIELD};
    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
        service_element_identifier_view, score::cpp::string_view{kInstanceSpecifier.ToString()}};

    // Expecting that a SkeletonField binding is created
    auto skeleton_field_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_field_binding_mock = *skeleton_field_binding_mock_ptr;
    EXPECT_CALL(skeleton_field_binding_factory_mock_guard.factory_mock_,
                CreateEventBinding(kInstanceIdentifier, _, score::cpp::string_view{kFieldName}))
        .WillOnce(Return(ByMove(std::move(skeleton_field_binding_mock_ptr))));
    EXPECT_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillOnce(Return(&tracing_mock));
    EXPECT_CALL(runtime_mock_guard.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));

    // and that RegisterServiceElement is called on the TracingRuntime binding in case TraceDoneCB relevant trace-points
    // are enabled or NOT called else!
    if (expected_enabled_trace_points.enable_send_with_allocate || expected_enabled_trace_points.enable_send)
    {
        EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));
    }
    else
    {
        EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa)).Times(0);
    }

    // and that GetBindingType is called on the binding
    EXPECT_CALL(skeleton_field_binding_mock, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    /// \todo When Instance id is supported by TracingFilterConfig, this instance_id should be properly filled
    // and expecting that status of each trace point is queried via the IcpTracingFilterConfig
    using SkeletonFieldTracePointType = tracing::SkeletonFieldTracePointType;
    EXPECT_CALL(tracing_mock,
                IsTracePointEnabled(service_type, score::cpp::string_view{kFieldName}, _, SkeletonFieldTracePointType::UPDATE))
        .WillOnce(Return(expected_enabled_trace_points.enable_send));
    EXPECT_CALL(tracing_mock,
                IsTracePointEnabled(
                    service_type, score::cpp::string_view{kFieldName}, _, SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE))
        .WillOnce(Return(expected_enabled_trace_points.enable_send_with_allocate));

    // Given a skeleton created based on a Lola binding
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdentifier};

    // Then all the trace points of the SkeletonEvent should be set according to the calls to IsTracePointEnabled
    const auto actual_enabled_trace_points = SkeletonFieldBaseView{unit.my_dummy_field_}.GetSkeletonEventTracing();
    EXPECT_EQ(actual_enabled_trace_points.service_element_instance_identifier_view,
              expected_service_element_instance_identifier_view);
    EXPECT_EQ(actual_enabled_trace_points.enable_send, expected_enabled_trace_points.enable_send);
    EXPECT_EQ(actual_enabled_trace_points.enable_send_with_allocate,
              expected_enabled_trace_points.enable_send_with_allocate);
}

SkeletonEventTracingData SkeletonEventTracingWithDefaultId(std::array<bool, 4> enable_flags) noexcept
{
    return SkeletonEventTracingData{tracing::ServiceElementInstanceIdentifierView{},
                                    enable_flags[0],
                                    enable_flags[1],
                                    enable_flags[2],
                                    enable_flags[3]};
}

INSTANTIATE_TEST_CASE_P(SkeletonFieldTracingParamaterisedFixture,
                        SkeletonFieldTracingParamaterisedFixture,
                        ::testing::Values(SkeletonEventTracingWithDefaultId({true, true, true, true}),
                                          SkeletonEventTracingWithDefaultId({false, false, false, false}),
                                          SkeletonEventTracingWithDefaultId({true, false, true, false}),
                                          SkeletonEventTracingWithDefaultId({true, false, false, false}),
                                          SkeletonEventTracingWithDefaultId({false, true, false, false}),
                                          SkeletonEventTracingWithDefaultId({false, false, true, false}),
                                          SkeletonEventTracingWithDefaultId({false, false, false, true})));

class SkeletonFieldTracingFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        // Expecting that a SkeletonField binding is created
        ExpectSkeletonServiceElementBindingCreation(skeleton_field_binding_factory_mock_guard_);
    }

    void CreateSkeleton()
    {
        // When a Skeleton containing a SkeletonField is created based on a lola deployment
        skeleton_ = std::make_unique<MyDummySkeleton>(std::make_unique<mock_binding::Skeleton>(), kInstanceIdentifier);
    }

    void ExpectSkeletonServiceElementBindingCreation(
        SkeletonFieldBindingFactoryMockGuard<TestSampleType>& factory_mock_guard)
    {
        auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
        mock_skeleton_field_binding_ = skeleton_event_binding_mock_ptr.get();
        EXPECT_CALL(factory_mock_guard.factory_mock_,
                    CreateEventBinding(kInstanceIdentifier, _, score::cpp::string_view{kFieldName}))
            .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));
    }

    tracing::ServiceElementInstanceIdentifierView CreateServiceElementInstanceIdentifierView() const noexcept
    {
        const tracing::ServiceElementIdentifierView service_element_identifier_view{
            kServiceTypeName, kFieldName, ServiceElementType::FIELD};
        const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
            service_element_identifier_view, score::cpp::string_view{kInstanceSpecifier.ToString()}};
        return expected_service_element_instance_identifier_view;
    }

    void ExpectIsTracePointEnabledCalls(const tracing::SkeletonEventTracingData& expected_enabled_trace_points,
                                        const score::cpp::string_view service_type,
                                        const score::cpp::string_view event_name,
                                        const score::cpp::string_view instance_specifier_view) const noexcept
    {
        const std::pair<tracing::SkeletonFieldTracePointType, bool> trace_points[] = {
            {tracing::SkeletonFieldTracePointType::UPDATE, expected_enabled_trace_points.enable_send},
            {tracing::SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE,
             expected_enabled_trace_points.enable_send_with_allocate}};
        for (const auto& items : trace_points)
        {
            EXPECT_CALL(tracing_filter_config_mock_,
                        IsTracePointEnabled(service_type, event_name, instance_specifier_view, items.first))
                .WillOnce(Return(items.second));
        }
    }

    bool AreTracePointsEqual(const tracing::SkeletonEventTracingData& lhs, const tracing::SkeletonEventTracingData& rhs)
    {
        EXPECT_EQ(lhs.enable_unsubscribe, rhs.enable_unsubscribe);
        EXPECT_EQ(lhs.enable_send, rhs.enable_send);
        EXPECT_EQ(lhs.enable_send_with_allocate, rhs.enable_send_with_allocate);
        return ((lhs.enable_send == rhs.enable_send) &&
                (lhs.enable_send_with_allocate == rhs.enable_send_with_allocate));
    }

    SkeletonFieldBindingFactoryMockGuard<TestSampleType> skeleton_field_binding_factory_mock_guard_{};
    std::unique_ptr<MyDummySkeleton> skeleton_{nullptr};
    tracing::RuntimeMockGuard runtime_mock_guard_{};
    tracing::TracingFilterConfigMock tracing_filter_config_mock_{};
    mock_binding::SkeletonEvent<TestSampleType>* mock_skeleton_field_binding_{nullptr};
};

using SkeletonFieldTracingSendFixture = SkeletonFieldTracingFixture;
TEST_F(SkeletonFieldTracingSendFixture, SendCallsAreTracedWhenEnabled)
{
    RecordProperty("Verifies", "SCR-18216878, SCR-18200105, SCR-18222321, SCR-18200787");
    RecordProperty("Description",
                   "The Trace point types for binding SkeletonField Send are correctly mapped "
                   "(SCR-18216878). The Send trace points are traced with a ShmDataChunkList "
                   "(SCR-18200105, SCR-18222321). The Send trace points are traced with a TracePointDataId.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send = true;

    const TestSampleType sample_data{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime twice (once on SkeletonEvent creation and once when
    // tracing) and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime())
        .Times(2)
        .WillRepeatedly(Return(&tracing_runtime_mock));
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with the Send trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kFieldName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result
    tracing::ITracingRuntime::TracePointType trace_point_type{tracing::SkeletonFieldTracePointType::UPDATE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      kTraceContextId,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      0U,
                      _,
                      _,
                      _))
        .WillOnce(WithArgs<6, 7>(Invoke([&sample_data](const void* data_ptr, std::size_t data_size) -> ResultBlank {
            EXPECT_EQ(data_size, sizeof(TestSampleType));
            const auto actual_data_ptr = static_cast<const TestSampleType*>(data_ptr);
            EXPECT_EQ(*actual_data_ptr, sample_data);
            return {};
        })));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(sample_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);
}

TEST_F(SkeletonFieldTracingSendFixture, SendTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    RecordProperty("Verifies", "SCR-18398059");
    RecordProperty("Description",
                   "The Trace point for binding SkeletonField Send should be disabled after receiving a "
                   "disable trace point error from the tracing runtime Trace call.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.enable_unsubscribe = true;
    expected_enabled_trace_points.enable_send_with_allocate = true;

    const TestSampleType sample_data{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime twice (once on SkeletonEvent creation and once when
    // tracing) and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime())
        .Times(2)
        .WillRepeatedly(Return(&tracing_runtime_mock));
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with the Send trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kFieldName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{tracing::SkeletonFieldTracePointType::UPDATE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      kTraceContextId,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      0U,
                      _,
                      _,
                      _))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(sample_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);

    // Then the specific trace point instance should now be disabled
    const auto actual_enabled_trace_points =
        SkeletonFieldBaseView{skeleton_->my_dummy_field_}.GetSkeletonEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_send = false;
    EXPECT_TRUE(AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TEST_F(SkeletonFieldTracingSendFixture, SendTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
{
    RecordProperty("Verifies", "SCR-18398059");
    RecordProperty("Description",
                   "All Trace points for the SkeletonEvent should be disabled after receiving a disable "
                   "all trace point error from the tracing runtime Trace call.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_unsubscribe = true;
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.enable_send_with_allocate = true;

    const TestSampleType sample_data{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime twice (once on SkeletonEvent creation and once when
    // tracing) and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime())
        .Times(2)
        .WillRepeatedly(Return(&tracing_runtime_mock));
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with the Send trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kFieldName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{tracing::SkeletonFieldTracePointType::UPDATE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      kTraceContextId,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      0U,
                      _,
                      _,
                      _))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(sample_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);

    // Then all trace point instances should now be disabled
    const auto& actual_enabled_trace_points =
        SkeletonFieldBaseView{skeleton_->my_dummy_field_}.GetSkeletonEventTracing();

    const SkeletonEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TEST_F(SkeletonFieldTracingSendFixture, SendCallsAreNotTracedWhenDisabled)
{
    RecordProperty("Verifies", "SCR-18217128");
    RecordProperty("Description",
                   "The binding SkeletonField Send trace points are not traced if the service element is "
                   "disabled in the Trace FilterConfig.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send = false;

    const TestSampleType sample_data{10U};

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with the Send trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kFieldName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is NOT called on the TracingRuntime binding, because no TraceDoneCB relevant
    // trace-points are enabled.
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa)).Times(0);

    // and that GetBindingType is called on the skeleton event binding once during SkeletonEvent creation
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should never be called

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(sample_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

TEST_F(SkeletonFieldTracingSendFixture, SendCallsAreNotTracedWhenTracingFilterConfigCannotBeRetrieved)
{
    RecordProperty("Verifies", "SCR-18217128, SCR-18159733");
    RecordProperty("Description",
                   "The binding SkeletonField Send trace points are not traced if the TraceFilterConfig "
                   "cannot be parsed/retrieved.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType sample_data{10U};

    // Expecting that the runtime returns a nullptr instead of a valid TracingFilterConfig
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig()).WillOnce(Return(nullptr));

    // and that the runtime returns a nullptr when getting the TracingRuntime
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));

    // and that the SkeletonEvent binding never checks which trace points are enabled

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should never be called

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(sample_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

TEST_F(SkeletonFieldTracingSendFixture, SendCallsAreNotTracedWhenTracingRuntimeCannotBeRetrieved)
{
    RecordProperty("Verifies", "SCR-18217128, SCR-18159733");
    RecordProperty(
        "Description",
        "The binding SkeletonField Send trace points are not traced if the TracingRuntime cannot be retrieved.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType sample_data{10U};

    // Expecting that the runtime returns a mocked TracingFilterConfig
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that the runtime returns a nullptr when getting the TracingRuntime
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(nullptr));

    // and that the SkeletonEvent binding never checks which trace points are enabled

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should never be called

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(sample_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

using SkeletonFieldTracingSendWithAllocateFixture = SkeletonFieldTracingFixture;
TEST_F(SkeletonFieldTracingSendWithAllocateFixture, SendCallsAreTracedWhenEnabled)
{
    RecordProperty("Verifies", "SCR-18216878, SCR-18200105, SCR-18222321, SCR-18200787");
    RecordProperty("Description",
                   "The Trace point types for binding SkeletonField Send with allocate are correctly mapped "
                   "(SCR-18216878). The Send trace points are traced with a ShmDataChunkList "
                   "(SCR-18200105, SCR-18222321). The Send trace points are traced with a TracePointDataId.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send_with_allocate = true;

    const TestSampleType initial_data{11U};
    const TestSampleType sample_data{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime twice (once on SkeletonEvent creation and once when
    // tracing) and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime())
        .Times(2)
        .WillRepeatedly(Return(&tracing_runtime_mock));
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with the Send trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kFieldName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result
    tracing::ITracingRuntime::TracePointType trace_point_type{
        tracing::SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      kTraceContextId,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      0U,
                      _,
                      _,
                      _))
        .WillOnce(WithArgs<6, 7>(Invoke([&sample_data](const void* data_ptr, std::size_t data_size) -> ResultBlank {
            EXPECT_EQ(data_size, sizeof(TestSampleType));
            const auto actual_data_ptr = static_cast<const TestSampleType*>(data_ptr);
            EXPECT_EQ(*actual_data_ptr, sample_data);
            return {};
        })));

    // and that Send() is called once on the event binding with the initial value
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(initial_data, _));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(initial_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_field_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_field_.Update(std::move(slot));

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);
}

TEST_F(SkeletonFieldTracingSendWithAllocateFixture,
       SendTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    RecordProperty("Verifies", "SCR-18398059");
    RecordProperty("Description",
                   "The Trace point for binding SkeletonField Send with allocate should be disabled after receiving a "
                   "disable trace point error from the tracing runtime Trace call.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send_with_allocate = true;
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.enable_unsubscribe = true;

    const TestSampleType initial_data{11U};
    const TestSampleType sample_data{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime twice (once on SkeletonEvent creation and once when
    // tracing) and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime())
        .Times(2)
        .WillRepeatedly(Return(&tracing_runtime_mock));
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with the Send trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kFieldName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{
        tracing::SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      kTraceContextId,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      0U,
                      _,
                      _,
                      _))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // and that Send() is called once on the event binding with the initial value
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(initial_data, _));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(initial_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_field_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_field_.Update(std::move(slot));

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);

    // Then the specific trace point instance should now be disabled
    const auto actual_enabled_trace_points =
        SkeletonFieldBaseView{skeleton_->my_dummy_field_}.GetSkeletonEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_send_with_allocate = false;
    EXPECT_TRUE(AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TEST_F(SkeletonFieldTracingSendWithAllocateFixture,
       SendTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
{
    RecordProperty("Verifies", "SCR-18398059");
    RecordProperty("Description",
                   "All Trace points for the SkeletonEvent should be disabled after receiving a disable "
                   "all trace point error from the tracing runtime Trace call.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send_with_allocate = true;
    expected_enabled_trace_points.enable_unsubscribe = true;
    expected_enabled_trace_points.enable_send = true;

    const TestSampleType initial_data{11U};
    const TestSampleType sample_data{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        CreateServiceElementInstanceIdentifierView();

    // Expecting that the runtime returns a mocked TracingRuntime twice (once on SkeletonEvent creation and once when
    // tracing) and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime())
        .Times(2)
        .WillRepeatedly(Return(&tracing_runtime_mock));
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with the Send trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kFieldName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{
        tracing::SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      kTraceContextId,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      0U,
                      _,
                      _,
                      _))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // and that Send() is called once on the event binding with the initial value
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(initial_data, _));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(initial_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_field_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_field_.Update(std::move(slot));

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);

    // Then all trace point instances should now be disabled
    const auto& actual_enabled_trace_points =
        SkeletonFieldBaseView{skeleton_->my_dummy_field_}.GetSkeletonEventTracing();

    const SkeletonEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TEST_F(SkeletonFieldTracingSendWithAllocateFixture, SendCallsAreNotTracedWhenDisabled)
{
    RecordProperty("Verifies", "SCR-18217128");
    RecordProperty("Description",
                   "The binding SkeletonField Send with allocate trace points are not traced if the service element is "
                   "disabled in the Trace FilterConfig.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send_with_allocate = false;

    const TestSampleType initial_data{11U};
    const TestSampleType sample_data{10U};

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with the Send trace point enabled.
    ExpectIsTracePointEnabledCalls(expected_enabled_trace_points,
                                   kServiceIdentifier.ToString(),
                                   score::cpp::string_view{kFieldName},
                                   score::cpp::string_view{kInstanceSpecifier.ToString()});

    // and that RegisterServiceElement is NOT called on the TracingRuntime binding, because no TraceDoneCB relevant
    // trace-points are enabled.
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa)).Times(0);

    // and that GetBindingType is called on the skeleton event binding once during SkeletonEvent creation
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should never be called

    // and that Send() is called once on the event binding with the initial value
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(initial_data, _));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(initial_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_field_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_field_.Update(std::move(slot));

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

TEST_F(SkeletonFieldTracingSendWithAllocateFixture, SendCallsAreNotTracedWhenTracingFilterConfigCannotBeRetrieved)
{
    RecordProperty("Verifies", "SCR-18217128, SCR-18159733");
    RecordProperty("Description",
                   "The binding SkeletonField Send with allocate trace points are not traced if the TraceFilterConfig "
                   "cannot be parsed/retrieved.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_data{11U};
    const TestSampleType sample_data{10U};

    // Expecting that the runtime returns a nullptr instead of a valid TracingFilterConfig
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig()).WillOnce(Return(nullptr));

    // and that the runtime returns a nullptr when getting the TracingRuntime
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));

    // and that the SkeletonEvent binding never checks which trace points are enabled

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should never be called

    // and that Send() is called once on the event binding with the initial value
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(initial_data, _));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(initial_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_field_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_field_.Update(std::move(slot));

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

TEST_F(SkeletonFieldTracingSendWithAllocateFixture, SendCallsAreNotTracedWhenTracingRuntimeCannotBeRetrieved)
{
    RecordProperty("Verifies", "SCR-18217128, SCR-18159733");
    RecordProperty(
        "Description",
        "The binding SkeletonField Send trace points are not traced if the TracingRuntime cannot be retrieved.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const TestSampleType initial_data{11U};
    const TestSampleType sample_data{10U};

    // Expecting that the runtime returns a mocked TracingFilterConfig
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that the runtime returns a nullptr when getting the TracingRuntime
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(nullptr));

    // and that the SkeletonEvent binding never checks which trace points are enabled

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should never be called

    // and that Send() is called once on the event binding with the initial value
    EXPECT_CALL(*mock_skeleton_field_binding_, Send(initial_data, _));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that PrepareStopOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_field_binding_, PrepareStopOffer());

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // When the initial value is set via an Update call
    skeleton_->my_dummy_field_.Update(initial_data);

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_field_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_field_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_field_.Update(std::move(slot));

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

}  // namespace
}  // namespace impl
}  // namespace com
}  // namespace mw
}  // namespace score
