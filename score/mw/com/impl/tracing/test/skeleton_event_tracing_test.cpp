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
#include "score/mw/com/impl/skeleton_event.h"

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

using SkeletonEventTracingData = tracing::SkeletonEventTracingData;

using TestSampleType = std::uint8_t;

const tracing::ITracingRuntimeBinding::TraceContextId kTraceContextId{0U};

const auto kEventName{"Event1"};

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

class MyDummySkeleton final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;

    SkeletonEvent<TestSampleType> my_dummy_event_{*this, kEventName};
};

TEST(SkeletonEventTracingTest, TracePointsAreDisabledIfConfigNotReturnedByRuntime)
{
    tracing::RuntimeMockGuard runtime_mock_guard{};
    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_, Create(kInstanceIdentifier, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));
    EXPECT_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillOnce(Return(nullptr));

    // Given a skeleton created based on FakeBindingInfo
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdentifier};

    const SkeletonEventTracingData actual_enabled_trace_points =
        SkeletonEventBaseView{unit.my_dummy_event_}.GetSkeletonEventTracing();
    EXPECT_EQ(actual_enabled_trace_points.service_element_instance_identifier_view,
              tracing::ServiceElementInstanceIdentifierView{});
    EXPECT_EQ(actual_enabled_trace_points.enable_send, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_send_with_allocate, false);
}

class SkeletonEventTracingParamaterisedFixture : public ::testing::TestWithParam<SkeletonEventTracingData>
{
};

TEST_P(SkeletonEventTracingParamaterisedFixture, TracePointsAreCorrectlySet)
{
    const SkeletonEventTracingData expected_enabled_trace_points = GetParam();

    tracing::RuntimeMockGuard runtime_mock_guard{};
    tracing::TracingFilterConfigMock tracing_mock{};
    tracing::TracingRuntimeMock tracing_runtime_mock{};

    const auto service_type = kServiceIdentifier.ToString();

    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard{};

    const tracing::ServiceElementIdentifierView service_element_identifier_view{
        service_type, kEventName, ServiceElementType::EVENT};
    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
        service_element_identifier_view, kInstanceSpecifier.ToString()};

    // Expecting that a SkeletonEvent binding is created
    auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
    auto& skeleton_event_binding_mock = *skeleton_event_binding_mock_ptr;
    EXPECT_CALL(skeleton_event_binding_factory_mock_guard.factory_mock_, Create(kInstanceIdentifier, _, kEventName))
        .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));
    EXPECT_CALL(runtime_mock_guard.runtime_mock_, GetTracingFilterConfig()).WillOnce(Return(&tracing_mock));
    EXPECT_CALL(runtime_mock_guard.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));

    // and that RegisterServiceElement is called on the GetTracingRuntime binding depending on SEND/SEND_WITH_ALLOCATE
    // settings
    const auto trace_done_cb_needed =
        expected_enabled_trace_points.enable_send_with_allocate || expected_enabled_trace_points.enable_send;

    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa)).Times(trace_done_cb_needed ? 1 : 0);

    // and that GetBindingType is called on the binding
    EXPECT_CALL(skeleton_event_binding_mock, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    /// \todo When Instance id is supported by TracingFilterConfig, this instance_id should be properly filled
    // and expecting that status of each trace point is queried via the IcpTracingFilterConfig
    using SkeletonEventTracePointType = tracing::SkeletonEventTracePointType;
    EXPECT_CALL(tracing_mock, IsTracePointEnabled(service_type, kEventName, _, SkeletonEventTracePointType::SEND))
        .WillOnce(Return(expected_enabled_trace_points.enable_send));
    EXPECT_CALL(tracing_mock,
                IsTracePointEnabled(service_type, kEventName, _, SkeletonEventTracePointType::SEND_WITH_ALLOCATE))
        .WillOnce(Return(expected_enabled_trace_points.enable_send_with_allocate));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    MyDummySkeleton unit{std::make_unique<mock_binding::Skeleton>(), kInstanceIdentifier};

    // Then all the trace points of the SkeletonEvent should be set according to the calls to IsTracePointEnabled
    const tracing::SkeletonEventTracingData actual_enabled_trace_points =
        SkeletonEventBaseView{unit.my_dummy_event_}.GetSkeletonEventTracing();
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

INSTANTIATE_TEST_CASE_P(SkeletonEventTracingParamaterisedFixture,
                        SkeletonEventTracingParamaterisedFixture,
                        ::testing::Values(SkeletonEventTracingWithDefaultId({true, true, true, true}),
                                          SkeletonEventTracingWithDefaultId({false, false, false, false}),
                                          SkeletonEventTracingWithDefaultId({true, false, true, false}),
                                          SkeletonEventTracingWithDefaultId({true, false, false, false}),
                                          SkeletonEventTracingWithDefaultId({false, true, false, false}),
                                          SkeletonEventTracingWithDefaultId({false, false, true, false}),
                                          SkeletonEventTracingWithDefaultId({false, false, false, true})));

class SkeletonEventTracingFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        // Expecting that a SkeletonEvent binding is created
        ExpectSkeletonServiceElementBindingCreation(skeleton_event_binding_factory_mock_guard_);
    }

    void CreateSkeleton()
    {
        // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
        skeleton_ = std::make_unique<MyDummySkeleton>(std::make_unique<mock_binding::Skeleton>(), kInstanceIdentifier);
    }

    void ExpectSkeletonServiceElementBindingCreation(
        SkeletonEventBindingFactoryMockGuard<TestSampleType>& factory_mock_guard)
    {
        auto skeleton_event_binding_mock_ptr = std::make_unique<mock_binding::SkeletonEvent<TestSampleType>>();
        mock_skeleton_event_binding_ = skeleton_event_binding_mock_ptr.get();
        EXPECT_CALL(factory_mock_guard.factory_mock_, Create(kInstanceIdentifier, _, kEventName))
            .WillOnce(Return(ByMove(std::move(skeleton_event_binding_mock_ptr))));
    }

    tracing::ServiceElementInstanceIdentifierView CreateServiceElementInstanceIdentifierView() const noexcept
    {
        const tracing::ServiceElementIdentifierView service_element_identifier_view{
            kServiceTypeName, kEventName, ServiceElementType::EVENT};
        const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
            service_element_identifier_view, kInstanceSpecifier.ToString()};
        return expected_service_element_instance_identifier_view;
    }

    void ExpectIsTracePointEnabledCalls(const tracing::SkeletonEventTracingData& expected_enabled_trace_points,
                                        const std::string_view service_type,
                                        const std::string_view event_name,
                                        const std::string_view instance_specifier_view) const noexcept
    {
        const std::pair<tracing::SkeletonEventTracePointType, bool> trace_points[] = {
            {tracing::SkeletonEventTracePointType::SEND, expected_enabled_trace_points.enable_send},
            {tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE,
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
        EXPECT_EQ(lhs.enable_send, rhs.enable_send);
        EXPECT_EQ(lhs.enable_send_with_allocate, rhs.enable_send_with_allocate);
        return ((lhs.enable_send == rhs.enable_send) &&
                (lhs.enable_send_with_allocate == rhs.enable_send_with_allocate));
    }

    SkeletonEventBindingFactoryMockGuard<TestSampleType> skeleton_event_binding_factory_mock_guard_{};
    std::unique_ptr<MyDummySkeleton> skeleton_{nullptr};
    tracing::RuntimeMockGuard runtime_mock_guard_{};
    tracing::TracingFilterConfigMock tracing_filter_config_mock_{};
    mock_binding::SkeletonEvent<TestSampleType>* mock_skeleton_event_binding_{nullptr};
};

using SkeletonEventTracingSendFixture = SkeletonEventTracingFixture;
TEST_F(SkeletonEventTracingSendFixture, SendCallsAreTracedWhenEnabled)
{
    RecordProperty("Verifies", "SCR-18216878, SCR-18200105, SCR-18222321, SCR-18200106, SCR-18200787");
    RecordProperty("Description",
                   "The Trace point types for binding SkeletonEvent Send are correctly mapped "
                   "(SCR-18216878). The Send trace points are traced with a ShmDataChunkList "
                   "(SCR-18200105, SCR-18222321, SCR-18200106). The Send trace points are traced with a "
                   "TracePointDataId (SCR-18200787).");
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
    ExpectIsTracePointEnabledCalls(
        expected_enabled_trace_points, kServiceIdentifier.ToString(), kEventName, kInstanceSpecifier.ToString());

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result
    tracing::ITracingRuntime::TracePointType trace_point_type{tracing::SkeletonEventTracePointType::SEND};
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

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(sample_data);

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);
}

TEST_F(SkeletonEventTracingSendFixture, SendTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    RecordProperty("Verifies", "SCR-18398059");
    RecordProperty("Description",
                   "The Trace point for binding SkeletonEvent Send should be disabled after receiving a "
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
    ExpectIsTracePointEnabledCalls(
        expected_enabled_trace_points, kServiceIdentifier.ToString(), kEventName, kInstanceSpecifier.ToString());

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{tracing::SkeletonEventTracePointType::SEND};
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

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(sample_data);

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);

    // Then the specific trace point instance should now be disabled
    const auto actual_enabled_trace_points =
        SkeletonEventBaseView{skeleton_->my_dummy_event_}.GetSkeletonEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_send = false;
    EXPECT_TRUE(AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TEST_F(SkeletonEventTracingSendFixture, SendTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
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
    ExpectIsTracePointEnabledCalls(
        expected_enabled_trace_points, kServiceIdentifier.ToString(), kEventName, kInstanceSpecifier.ToString());

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{tracing::SkeletonEventTracePointType::SEND};
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

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(sample_data);

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);

    // Then all trace point instances should now be disabled
    const auto& actual_enabled_trace_points =
        SkeletonEventBaseView{skeleton_->my_dummy_event_}.GetSkeletonEventTracing();

    const SkeletonEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TEST_F(SkeletonEventTracingSendFixture, SendCallsAreNotTracedWhenDisabled)
{
    RecordProperty("Verifies", "SCR-18217128");
    RecordProperty("Description",
                   "The binding SkeletonEvent Send trace points are not traced if the service element is "
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
    ExpectIsTracePointEnabledCalls(
        expected_enabled_trace_points, kServiceIdentifier.ToString(), kEventName, kInstanceSpecifier.ToString());

    // and that RegisterServiceElement is NOT called on the TracingRuntime binding, because no TraceDoneCB relevant
    // trace-points are enabled.
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa)).Times(0);

    // and that GetBindingType is called on the skeleton event binding once during SkeletonEvent creation
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // Then a trace call relating to Send should never be called

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(sample_data);

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

TEST_F(SkeletonEventTracingSendFixture, SendCallsAreNotTracedWhenTracingFilterConfigCannotBeRetrieved)
{
    RecordProperty("Verifies", "SCR-18217128, SCR-18159733");
    RecordProperty("Description",
                   "The binding SkeletonEvent Send trace points are not traced if the TraceFilterConfig "
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
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // Then a trace call relating to Send should never be called

    // and that GetBindingType is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(sample_data);

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

TEST_F(SkeletonEventTracingSendFixture, SendCallsAreNotTracedWhenTracingRuntimeCannotBeRetrieved)
{
    RecordProperty("Verifies", "SCR-18217128, SCR-18159733");
    RecordProperty(
        "Description",
        "The binding SkeletonEvent Send trace points are not traced if the TracingRuntime cannot be retrieved.");
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
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(sample_data, _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // Then a trace call relating to Send should never be called

    // and that GetBindingType is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(sample_data);

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

using SkeletonEventTracingSendWithAllocateFixture = SkeletonEventTracingFixture;
TEST_F(SkeletonEventTracingSendWithAllocateFixture, SendCallsAreTracedWhenEnabled)
{
    RecordProperty("Verifies", "SCR-18216878, SCR-18200105, SCR-18222321, SCR-18200106, SCR-18200787");
    RecordProperty("Description",
                   "The Trace point types for binding SkeletonEvent Send with allocate are correctly mapped "
                   "(SCR-18216878). The Send trace points are traced with a ShmDataChunkList "
                   "(SCR-18200105, SCR-18222321, SCR-18200106). The Send trace points are traced with a "
                   "TracePointDataId (SCR-18200787).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
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
    ExpectIsTracePointEnabledCalls(
        expected_enabled_trace_points, kServiceIdentifier.ToString(), kEventName, kInstanceSpecifier.ToString());

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result
    tracing::ITracingRuntime::TracePointType trace_point_type{tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE};
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

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_event_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(std::move(slot));

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);
}

TEST_F(SkeletonEventTracingSendWithAllocateFixture,
       SendTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    RecordProperty("Verifies", "SCR-18398059");
    RecordProperty("Description",
                   "The Trace point for binding SkeletonEvent Send with allocate should be disabled after receiving a "
                   "disable trace point error from the tracing runtime Trace call.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send_with_allocate = true;
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.enable_unsubscribe = true;

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
    ExpectIsTracePointEnabledCalls(
        expected_enabled_trace_points, kServiceIdentifier.ToString(), kEventName, kInstanceSpecifier.ToString());

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE};
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

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_event_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(std::move(slot));

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);

    // Then the specific trace point instance should now be disabled
    const auto actual_enabled_trace_points =
        SkeletonEventBaseView{skeleton_->my_dummy_event_}.GetSkeletonEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_send_with_allocate = false;
    EXPECT_TRUE(AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TEST_F(SkeletonEventTracingSendWithAllocateFixture,
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
    ExpectIsTracePointEnabledCalls(
        expected_enabled_trace_points, kServiceIdentifier.ToString(), kEventName, kInstanceSpecifier.ToString());

    // and that RegisterServiceElement is called on the GetTracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa));

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // Then a trace call relating to Send should be called containing the correct max_sample_count and
    // subscription result which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE};
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

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // and that GetBindingType is called on the skeleton event binding twice (once in SkeletonEvent creation and once
    // when calling tracing)
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).Times(2).WillRepeatedly(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_event_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(std::move(slot));

    // and the send tracing callback is called
    auto ptr = MakeSampleAllocateePtr(std::make_unique<TestSampleType>(sample_data));
    ASSERT_TRUE(send_trace_callback_result.has_value());
    (*send_trace_callback_result)(ptr);

    // Then all trace point instances should now be disabled
    const auto& actual_enabled_trace_points =
        SkeletonEventBaseView{skeleton_->my_dummy_event_}.GetSkeletonEventTracing();

    const SkeletonEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TEST_F(SkeletonEventTracingSendWithAllocateFixture, SendCallsAreNotTracedWhenDisabled)
{
    RecordProperty("Verifies", "SCR-18217128");
    RecordProperty("Description",
                   "The binding SkeletonEvent Send with allocate trace points are not traced if the service element is "
                   "disabled in the Trace FilterConfig.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send_with_allocate = false;

    const TestSampleType sample_data{10U};

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(tracing_filter_config_mock_)));

    // and that a SkeletonEvent binding is created with the Send trace point enabled.
    ExpectIsTracePointEnabledCalls(
        expected_enabled_trace_points, kServiceIdentifier.ToString(), kEventName, kInstanceSpecifier.ToString());

    // and that RegisterServiceElement is NOT called on the TracingRuntime binding, because no TraceDoneCB relevant
    // trace-points are enabled.
    EXPECT_CALL(tracing_runtime_mock, RegisterServiceElement(BindingType::kLoLa)).Times(0);

    // and that GetBindingType is called on the skeleton event binding once during SkeletonEvent creation
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // Then a trace call relating to Send should never be called

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_event_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(std::move(slot));

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

TEST_F(SkeletonEventTracingSendWithAllocateFixture, SendCallsAreNotTracedWhenTracingFilterConfigCannotBeRetrieved)
{
    RecordProperty("Verifies", "SCR-18217128, SCR-18159733");
    RecordProperty("Description",
                   "The binding SkeletonEvent Send with allocate trace points are not traced if the TraceFilterConfig "
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

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // Then a trace call relating to Send should never be called

    // and that GetBindingType is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_event_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(std::move(slot));

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

TEST_F(SkeletonEventTracingSendWithAllocateFixture, SendCallsAreNotTracedWhenTracingRuntimeCannotBeRetrieved)
{
    RecordProperty("Verifies", "SCR-18217128, SCR-18159733");
    RecordProperty(
        "Description",
        "The binding SkeletonEvent Send trace points are not traced if the TracingRuntime cannot be retrieved.");
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

    // and that Allocate() is called once on the event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, Allocate())
        .WillOnce(Return(ByMove(MakeSampleAllocateePtr(std::make_unique<TestSampleType>()))));

    // and that Send will be called on the binding with the wrapped handler containing the trace call
    score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback> send_trace_callback_result{};
    EXPECT_CALL(*mock_skeleton_event_binding_, Send(An<SampleAllocateePtr<TestSampleType>>(), _))
        .WillOnce(WithArgs<1>(
            Invoke([&send_trace_callback_result](score::cpp::optional<SkeletonEventBinding<TestSampleType>::SendTraceCallback>
                                                     provided_send_trace_callback) -> ResultBlank {
                send_trace_callback_result = std::move(provided_send_trace_callback);
                return {};
            })));

    // and that PrepareOffer is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, PrepareOffer());

    // Then a trace call relating to Send should never be called

    // and that GetBindingType is called on the skeleton event binding
    EXPECT_CALL(*mock_skeleton_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Skeleton containing a SkeletonEvent is created based on a lola deployment
    CreateSkeleton();

    // and PrepareOffer is called on the event
    skeleton_->my_dummy_event_.PrepareOffer();

    // and Allocate is called on the event.
    auto slot_result = skeleton_->my_dummy_event_.Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();
    *slot = sample_data;

    // and Send is called on the event
    skeleton_->my_dummy_event_.Send(std::move(slot));

    // and the wrapped handler is an empty optional
    ASSERT_FALSE(send_trace_callback_result.has_value());
}

}  // namespace
}  // namespace impl
}  // namespace com
}  // namespace mw
}  // namespace score
