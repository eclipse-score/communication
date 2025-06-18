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
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_field.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/proxy_resources.h"
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

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

using TestSampleType = std::uint16_t;

const auto kInstanceSpecifier{InstanceSpecifier::Create("abc/abc/TirePressurePort").value()};
const auto kInstanceSpecifierStdStringView = kInstanceSpecifier.ToString();
const auto kInstanceSpecifierStringView =
    score::cpp::string_view{kInstanceSpecifierStdStringView.data(), kInstanceSpecifierStdStringView.size()};
const std::string kServiceTypeName{"foo"};
const ServiceIdentifierType kServiceIdentifier{make_ServiceIdentifierType(kServiceTypeName, 13, 37)};
const auto kServiceIdentifierStdStringView = kServiceIdentifier.ToString();
const auto kServiceIdentifierStringView =
    score::cpp::string_view{kServiceIdentifierStdStringView.data(), kServiceIdentifierStdStringView.size()};
const ServiceInstanceDeployment kInstanceDeployment{kServiceIdentifier,
                                                    LolaServiceInstanceDeployment{LolaServiceInstanceId{23U}},
                                                    QualityType::kASIL_QM,
                                                    kInstanceSpecifier};
const ServiceTypeDeployment kTypeDeployment{LolaServiceTypeDeployment{34U}};
const InstanceIdentifier kInstanceIdentifier{make_InstanceIdentifier(kInstanceDeployment, kTypeDeployment)};
const HandleType kHandle{make_HandleType(kInstanceIdentifier)};

using ProxyEventTracingData = tracing::ProxyEventTracingData;

const auto kServiceElementName{"ServiceElement1"};

class MyDummyProxyWithEvent : public ProxyBase
{
  public:
    using ProxyBase::ProxyBase;

    ProxyEvent<TestSampleType> my_service_element_{*this, kServiceElementName};
};

class MyDummyProxyWithField : public ProxyBase
{
  public:
    using ProxyBase::ProxyBase;

    ProxyField<TestSampleType> my_service_element_{*this, kServiceElementName};
};

/// \brief Structs containing types for templated gtests.
///
/// We use a struct instead of a tuple since a tuple cannot contain a void type.
struct ProxyEventStruct
{
    using ProxyType = MyDummyProxyWithEvent;
    using ProxyServiceElementBindingFactoryMockGuard = ProxyEventBindingFactoryMockGuard<TestSampleType>;
    using TracePointType = tracing::ProxyEventTracePointType;
};

struct ProxyFieldStruct
{
    using ProxyType = MyDummyProxyWithField;
    using ProxyServiceElementBindingFactoryMockGuard = ProxyFieldBindingFactoryMockGuard<TestSampleType>;
    using TracePointType = tracing::ProxyFieldTracePointType;
};

template <typename T>
class ProxyEventTracingFixture : public ::testing::Test
{
  public:
    using ProxyType = typename T::ProxyType;
    using ProxyServiceElementBindingFactoryMockGuard = typename T::ProxyServiceElementBindingFactoryMockGuard;
    using TracePointType = typename T::TracePointType;

    void SetUp() override
    {
        // Expecting that a ProxyEvent binding is created
        ExpectProxyServiceElementBindingCreation(proxy_service_element_binding_factory_mock_guard_);

        ON_CALL(*mock_proxy_event_binding_, SetReceiveHandler(_)).WillByDefault(Return(score::cpp::blank{}));
    }

    void CreateProxy()
    {
        // When a Proxy containing a ProxyEvent is created based on a lola deployment
        proxy_ = std::make_unique<ProxyType>(std::make_unique<mock_binding::Proxy>(), kHandle);
    }

    void ExpectProxyServiceElementBindingCreation(ProxyEventBindingFactoryMockGuard<TestSampleType>& factory_mock_guard)
    {
        auto proxy_event_binding_mock_ptr = std::make_unique<mock_binding::ProxyEvent<TestSampleType>>();
        mock_proxy_event_binding_ = proxy_event_binding_mock_ptr.get();
        EXPECT_CALL(factory_mock_guard.factory_mock_, Create(_, score::cpp::string_view{kServiceElementName}))
            .WillOnce(Return(ByMove(std::move(proxy_event_binding_mock_ptr))));
    }

    void ExpectProxyServiceElementBindingCreation(ProxyFieldBindingFactoryMockGuard<TestSampleType>& factory_mock_guard)
    {
        auto proxy_event_binding_mock_ptr = std::make_unique<mock_binding::ProxyEvent<TestSampleType>>();
        mock_proxy_event_binding_ = proxy_event_binding_mock_ptr.get();
        EXPECT_CALL(factory_mock_guard.factory_mock_, CreateEventBinding(_, score::cpp::string_view{kServiceElementName}))
            .WillOnce(Return(ByMove(std::move(proxy_event_binding_mock_ptr))));
    }

    tracing::ServiceElementInstanceIdentifierView CreateServiceElementInstanceIdentifierView(
        tracing::ProxyEventTracePointType) const noexcept
    {
        const tracing::ServiceElementIdentifierView service_element_identifier_view{
            kServiceTypeName, kServiceElementName, tracing::ServiceElementType::EVENT};
        const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
            service_element_identifier_view, kInstanceSpecifierStringView};
        return expected_service_element_instance_identifier_view;
    }

    tracing::ServiceElementInstanceIdentifierView CreateServiceElementInstanceIdentifierView(
        tracing::ProxyFieldTracePointType) const noexcept
    {
        const tracing::ServiceElementIdentifierView service_element_identifier_view{
            kServiceTypeName, kServiceElementName, tracing::ServiceElementType::FIELD};
        const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
            service_element_identifier_view, kInstanceSpecifierStringView};
        return expected_service_element_instance_identifier_view;
    }

    template <typename TracePointType>
    void ExpectIsTracePointEnabledCalls(const tracing::ProxyEventTracingData& expected_enabled_trace_points,
                                        const score::cpp::string_view service_type,
                                        const score::cpp::string_view event_name,
                                        const score::cpp::string_view instance_specifier_view) const noexcept
    {
        const std::pair<TracePointType, bool> trace_points[] = {
            {TracePointType::SUBSCRIBE, expected_enabled_trace_points.enable_subscribe},
            {TracePointType::UNSUBSCRIBE, expected_enabled_trace_points.enable_unsubscribe},
            {TracePointType::SUBSCRIBE_STATE_CHANGE, expected_enabled_trace_points.enable_subscription_state_changed},
            {TracePointType::SET_SUBSCRIPTION_STATE_CHANGE_HANDLER,
             expected_enabled_trace_points.enable_set_subcription_state_change_handler},
            {TracePointType::UNSET_SUBSCRIPTION_STATE_CHANGE_HANDLER,
             expected_enabled_trace_points.enable_unset_subscription_state_change_handler},
            {TracePointType::SUBSCRIPTION_STATE_CHANGE_HANDLER_CALLBACK,
             expected_enabled_trace_points.enable_call_subscription_state_change_handler},
            {TracePointType::SET_RECEIVE_HANDLER, expected_enabled_trace_points.enable_set_receive_handler},
            {TracePointType::UNSET_RECEIVE_HANDLER, expected_enabled_trace_points.enable_unset_receive_handler},
            {TracePointType::RECEIVE_HANDLER_CALLBACK, expected_enabled_trace_points.enable_call_receive_handler},
            {TracePointType::GET_NEW_SAMPLES, expected_enabled_trace_points.enable_get_new_samples},
            {TracePointType::GET_NEW_SAMPLES_CALLBACK, expected_enabled_trace_points.enable_new_samples_callback}};
        for (const auto& items : trace_points)
        {
            EXPECT_CALL(tracing_filter_config_mock_,
                        IsTracePointEnabled(service_type, event_name, instance_specifier_view, items.first))
                .WillOnce(Return(items.second));
        }
    }

    bool AreTracePointsEqual(const tracing::ProxyEventTracingData& lhs, const tracing::ProxyEventTracingData& rhs)
    {
        EXPECT_EQ(lhs.enable_subscribe, rhs.enable_subscribe);
        EXPECT_EQ(lhs.enable_unsubscribe, rhs.enable_unsubscribe);
        EXPECT_EQ(lhs.enable_subscription_state_changed, rhs.enable_subscription_state_changed);
        EXPECT_EQ(lhs.enable_set_subcription_state_change_handler, rhs.enable_set_subcription_state_change_handler);
        EXPECT_EQ(lhs.enable_unset_subscription_state_change_handler,
                  rhs.enable_unset_subscription_state_change_handler);
        EXPECT_EQ(lhs.enable_call_subscription_state_change_handler, rhs.enable_call_subscription_state_change_handler);
        EXPECT_EQ(lhs.enable_set_receive_handler, rhs.enable_set_receive_handler);
        EXPECT_EQ(lhs.enable_unset_receive_handler, rhs.enable_unset_receive_handler);
        EXPECT_EQ(lhs.enable_call_receive_handler, rhs.enable_call_receive_handler);
        EXPECT_EQ(lhs.enable_get_new_samples, rhs.enable_get_new_samples);
        EXPECT_EQ(lhs.enable_new_samples_callback, rhs.enable_new_samples_callback);
        return (
            (lhs.enable_subscribe == rhs.enable_subscribe) && (lhs.enable_unsubscribe == rhs.enable_unsubscribe) &&
            (lhs.enable_subscription_state_changed == rhs.enable_subscription_state_changed) &&
            (lhs.enable_set_subcription_state_change_handler == rhs.enable_set_subcription_state_change_handler) &&
            (lhs.enable_unset_subscription_state_change_handler ==
             rhs.enable_unset_subscription_state_change_handler) &&
            (lhs.enable_call_subscription_state_change_handler == rhs.enable_call_subscription_state_change_handler) &&
            (lhs.enable_set_receive_handler == rhs.enable_set_receive_handler) &&
            (lhs.enable_unset_receive_handler == rhs.enable_unset_receive_handler) &&
            (lhs.enable_call_receive_handler == rhs.enable_call_receive_handler) &&
            (lhs.enable_get_new_samples == rhs.enable_get_new_samples) &&
            (lhs.enable_new_samples_callback == rhs.enable_new_samples_callback));
    }

    ProxyServiceElementBindingFactoryMockGuard proxy_service_element_binding_factory_mock_guard_{};
    std::unique_ptr<ProxyType> proxy_{nullptr};
    tracing::RuntimeMockGuard runtime_mock_guard_{};
    tracing::TracingFilterConfigMock tracing_filter_config_mock_{};
    mock_binding::ProxyEvent<TestSampleType>* mock_proxy_event_binding_{nullptr};
};

// Gtest will run all tests in the ProxyEventTracingFixture once for every type, t, in MyTypes, such that TypeParam
// == t for each run.
using MyTypes = ::testing::Types<ProxyEventStruct, ProxyFieldStruct>;
TYPED_TEST_SUITE(ProxyEventTracingFixture, MyTypes, );

template <typename T>
using ProxyEventTracingEnabledTracePointsFixture = ProxyEventTracingFixture<T>;
TYPED_TEST_SUITE(ProxyEventTracingEnabledTracePointsFixture, MyTypes, );

template <typename T>
using ProxyEventTracingSubscribeFixture = ProxyEventTracingFixture<T>;
TYPED_TEST_SUITE(ProxyEventTracingSubscribeFixture, MyTypes, );

template <typename T>
using ProxyEventTracingUnsubscribeFixture = ProxyEventTracingFixture<T>;
TYPED_TEST_SUITE(ProxyEventTracingUnsubscribeFixture, MyTypes, );

template <typename T>
using ProxyEventTracingSetReceiveHandlerFixture = ProxyEventTracingFixture<T>;
TYPED_TEST_SUITE(ProxyEventTracingSetReceiveHandlerFixture, MyTypes, );

template <typename T>
using ProxyEventTracingReceiveHandlerCallbackFixture = ProxyEventTracingFixture<T>;
TYPED_TEST_SUITE(ProxyEventTracingReceiveHandlerCallbackFixture, MyTypes, );

template <typename T>
using ProxyEventTracingUnsetReceiveHandlerFixture = ProxyEventTracingFixture<T>;
TYPED_TEST_SUITE(ProxyEventTracingUnsetReceiveHandlerFixture, MyTypes, );

template <typename T>
using ProxyEventTracingGetNewSamplesFixture = ProxyEventTracingFixture<T>;
TYPED_TEST_SUITE(ProxyEventTracingGetNewSamplesFixture, MyTypes, );

template <typename T>
using ProxyEventTracingGetNewSamplesCallbackFixture = ProxyEventTracingFixture<T>;
TYPED_TEST_SUITE(ProxyEventTracingGetNewSamplesCallbackFixture, MyTypes, );

TYPED_TEST(ProxyEventTracingEnabledTracePointsFixture, TracePointsAreDisabledIfConfigNotReturnedByRuntime)
{
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig()).WillOnce(Return(nullptr));

    // Given a proxy containing a Service Element which is connected to a mock binding
    this->CreateProxy();

    // Then all the trace points of the ProxyEvent should be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    EXPECT_EQ(actual_enabled_trace_points.service_element_instance_identifier_view,
              tracing::ServiceElementInstanceIdentifierView{});
    EXPECT_EQ(actual_enabled_trace_points.enable_subscribe, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_unsubscribe, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_subscription_state_changed, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_set_subcription_state_change_handler, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_unset_subscription_state_change_handler, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_call_subscription_state_change_handler, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_set_receive_handler, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_unset_receive_handler, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_call_receive_handler, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_get_new_samples, false);
    EXPECT_EQ(actual_enabled_trace_points.enable_new_samples_callback, false);
}

class ProxyEventTracingEnabledTracePointsParamaterizedFixture
    : public ProxyEventTracingFixture<ProxyEventStruct>,
      public ::testing::WithParamInterface<ProxyEventTracingData>
{
};

class ProxyFieldTracingEnabledTracePointsParamaterizedFixture
    : public ProxyEventTracingFixture<ProxyFieldStruct>,
      public ::testing::WithParamInterface<ProxyEventTracingData>
{
};

TEST_P(ProxyEventTracingEnabledTracePointsParamaterizedFixture, TracePointsAreCorrectlySet)
{
    const ProxyEventTracingData expected_enabled_trace_points = GetParam();

    const tracing::ServiceElementIdentifierView service_element_identifier_view{
        kServiceTypeName, kServiceElementName, tracing::ServiceElementType::EVENT};
    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
        service_element_identifier_view, kInstanceSpecifierStringView};

    // Expecting that the runtime returns a valid TracingFilterConfig
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&tracing_filter_config_mock_));

    // and that a ProxyEvent binding is created and is filled by calling IsTracePointEnabled()
    ExpectIsTracePointEnabledCalls<tracing::ProxyEventTracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Given a proxy containing a Service Element which is connected to a mock binding
    CreateProxy();

    // Then all the trace points of the ProxyEvent should be set according to the calls to IsTracePointEnabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{proxy_->my_service_element_}.GetProxyEventTracing();
    EXPECT_EQ(actual_enabled_trace_points.service_element_instance_identifier_view,
              expected_service_element_instance_identifier_view);
    EXPECT_EQ(actual_enabled_trace_points.enable_subscribe, expected_enabled_trace_points.enable_subscribe);
    EXPECT_EQ(actual_enabled_trace_points.enable_unsubscribe, expected_enabled_trace_points.enable_unsubscribe);
    EXPECT_EQ(actual_enabled_trace_points.enable_subscription_state_changed,
              expected_enabled_trace_points.enable_subscription_state_changed);
    EXPECT_EQ(actual_enabled_trace_points.enable_set_subcription_state_change_handler,
              expected_enabled_trace_points.enable_set_subcription_state_change_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_unset_subscription_state_change_handler,
              expected_enabled_trace_points.enable_unset_subscription_state_change_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_call_subscription_state_change_handler,
              expected_enabled_trace_points.enable_call_subscription_state_change_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_set_receive_handler,
              expected_enabled_trace_points.enable_set_receive_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_unset_receive_handler,
              expected_enabled_trace_points.enable_unset_receive_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_call_receive_handler,
              expected_enabled_trace_points.enable_call_receive_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_get_new_samples, expected_enabled_trace_points.enable_get_new_samples);
    EXPECT_EQ(actual_enabled_trace_points.enable_new_samples_callback,
              expected_enabled_trace_points.enable_new_samples_callback);
}

TEST_P(ProxyFieldTracingEnabledTracePointsParamaterizedFixture, TracePointsAreCorrectlySet)
{
    const ProxyEventTracingData expected_enabled_trace_points = GetParam();

    const tracing::ServiceElementIdentifierView service_element_identifier_view{
        kServiceTypeName, kServiceElementName, tracing::ServiceElementType::FIELD};
    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
        service_element_identifier_view, kInstanceSpecifierStringView};

    // Expecting that the runtime returns a valid TracingFilterConfig
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&tracing_filter_config_mock_));

    // Expecting that a ProxyEvent binding is created and is filled by calling IsTracePointEnabled()
    ExpectIsTracePointEnabledCalls<tracing::ProxyFieldTracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Given a proxy containing a Service Element which is connected to a mock binding
    CreateProxy();

    // Then all the trace points of the ProxyEvent should be set according to the calls to IsTracePointEnabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{proxy_->my_service_element_}.GetProxyEventTracing();
    EXPECT_EQ(actual_enabled_trace_points.service_element_instance_identifier_view,
              expected_service_element_instance_identifier_view);
    EXPECT_EQ(actual_enabled_trace_points.enable_subscribe, expected_enabled_trace_points.enable_subscribe);
    EXPECT_EQ(actual_enabled_trace_points.enable_unsubscribe, expected_enabled_trace_points.enable_unsubscribe);
    EXPECT_EQ(actual_enabled_trace_points.enable_subscription_state_changed,
              expected_enabled_trace_points.enable_subscription_state_changed);
    EXPECT_EQ(actual_enabled_trace_points.enable_set_subcription_state_change_handler,
              expected_enabled_trace_points.enable_set_subcription_state_change_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_unset_subscription_state_change_handler,
              expected_enabled_trace_points.enable_unset_subscription_state_change_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_call_subscription_state_change_handler,
              expected_enabled_trace_points.enable_call_subscription_state_change_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_set_receive_handler,
              expected_enabled_trace_points.enable_set_receive_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_unset_receive_handler,
              expected_enabled_trace_points.enable_unset_receive_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_call_receive_handler,
              expected_enabled_trace_points.enable_call_receive_handler);
    EXPECT_EQ(actual_enabled_trace_points.enable_get_new_samples, expected_enabled_trace_points.enable_get_new_samples);
    EXPECT_EQ(actual_enabled_trace_points.enable_new_samples_callback,
              expected_enabled_trace_points.enable_new_samples_callback);
}

ProxyEventTracingData ProxyEventTracingWithDefaultId(std::array<bool, 11> enable_flags) noexcept
{
    return ProxyEventTracingData{tracing::ServiceElementInstanceIdentifierView{},
                                 enable_flags[0],
                                 enable_flags[1],
                                 enable_flags[2],
                                 enable_flags[3],
                                 enable_flags[4],
                                 enable_flags[5],
                                 enable_flags[6],
                                 enable_flags[7],
                                 enable_flags[8],
                                 enable_flags[9],
                                 enable_flags[10]};
}

INSTANTIATE_TEST_CASE_P(
    ProxyEventTracingEnabledTracePointsParamaterizedFixture,
    ProxyEventTracingEnabledTracePointsParamaterizedFixture,
    ::testing::Values(
        ProxyEventTracingWithDefaultId({true, true, true, true, true, true, true, true, true, true, true}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({true, false, true, false, true, false, true, false, true, false, true}),
        ProxyEventTracingWithDefaultId({true, false, false, false, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, true, false, false, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, true, false, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, true, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, true, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, true, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, true, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, true, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, false, true, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, false, false, true, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, false, false, false, true})));
INSTANTIATE_TEST_CASE_P(
    ProxyFieldTracingEnabledTracePointsParamaterizedFixture,
    ProxyFieldTracingEnabledTracePointsParamaterizedFixture,
    ::testing::Values(
        ProxyEventTracingWithDefaultId({true, true, true, true, true, true, true, true, true, true, true}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({true, false, true, false, true, false, true, false, true, false, true}),
        ProxyEventTracingWithDefaultId({true, false, false, false, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, true, false, false, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, true, false, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, true, false, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, true, false, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, true, false, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, true, false, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, true, false, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, false, true, false, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, false, false, true, false}),
        ProxyEventTracingWithDefaultId({false, false, false, false, false, false, false, false, false, false, true})));

TYPED_TEST(ProxyEventTracingSubscribeFixture, SubscribeCallsAreTracedWhenEnabled)
{
    this->RecordProperty("Verifies", "SCR-18216878, SCR-18221771, SCR-18222516");
    this->RecordProperty(
        "Description",
        "The Trace point types for ProxyEvent/ProxyField Subscibe are correctly mapped (SCR-18216878). The Subscribe "
        "trace points are traced with a LocalDataChunkList (SCR-18221771, SCR-18222516).");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_subscribe = true;

    const std::size_t max_sample_count{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    StrictMock<tracing::TracingRuntimeMock> tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the Subscribe trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to Subscribe should be called containing the correct max_sample_count
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::SUBSCRIBE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      _,
                      _))
        .WillOnce(WithArgs<4, 5>(
            Invoke([max_sample_count](const void* local_data_ptr, std::size_t local_data_size) -> ResultBlank {
                EXPECT_EQ(local_data_size, sizeof(std::size_t));
                const auto actual_max_sample_count_ptr = static_cast<const std::size_t*>(local_data_ptr);
                EXPECT_EQ(max_sample_count, *actual_max_sample_count_ptr);
                return {};
            })));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that GetSubscriptionState is called once and indicates that the Service Element is currently not
    // subscribed
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kNotSubscribed));

    // and that Subscribe will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, Subscribe(max_sample_count));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and subscribe is called on the event
    this->proxy_->my_service_element_.Subscribe(max_sample_count);
}

TYPED_TEST(ProxyEventTracingSubscribeFixture,
           SubscribeTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "The Trace point for ProxyEvent/ProxyField Subscibe should be disabled after receiving a "
                         "disable trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_unsubscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const std::size_t max_sample_count{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the Subscribe trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to Subscribe should be called containing the correct max_sample_count which returns an
    // error
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::SUBSCRIBE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      _,
                      _))
        .WillOnce(WithArgs<4, 5>(
            Invoke([max_sample_count](const void* local_data_ptr, std::size_t local_data_size) -> ResultBlank {
                EXPECT_EQ(local_data_size, sizeof(std::size_t));
                const auto actual_max_sample_count_ptr = static_cast<const std::size_t*>(local_data_ptr);
                EXPECT_EQ(max_sample_count, *actual_max_sample_count_ptr);
                return MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableTracePointInstance);
            })));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that GetSubscriptionState is called once and indicates that the Service Element is currently not
    // subscribed
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kNotSubscribed));

    // and that Subscribe will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, Subscribe(max_sample_count));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and subscribe is called on the event
    this->proxy_->my_service_element_.Subscribe(max_sample_count);

    // Then the specific trace point instance should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_subscribe = false;
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingSubscribeFixture,
           SubscribeTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "All Trace points for the ProxyEvent/ProxyField should be disabled after receiving a disable "
                         "all trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_unsubscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const std::size_t max_sample_count{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the Subscribe trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to Subscribe should be called containing the correct max_sample_count which returns an
    // error
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::SUBSCRIBE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      _,
                      _))
        .WillOnce(WithArgs<4, 5>(
            Invoke([max_sample_count](const void* local_data_ptr, std::size_t local_data_size) -> ResultBlank {
                EXPECT_EQ(local_data_size, sizeof(std::size_t));
                const auto actual_max_sample_count_ptr = static_cast<const std::size_t*>(local_data_ptr);
                EXPECT_EQ(max_sample_count, *actual_max_sample_count_ptr);
                return MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableAllTracePoints);
            })));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that GetSubscriptionState is called once and indicates that the Service Element is currently not
    // subscribed
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kNotSubscribed));

    // and that Subscribe will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, Subscribe(max_sample_count));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and subscribe is called on the event
    this->proxy_->my_service_element_.Subscribe(max_sample_count);

    // Then all trace point instances should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    const ProxyEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingSubscribeFixture, SubscribeCallsAreNotTracedWhenDisabled)
{
    this->RecordProperty("Verifies", "SCR-18217128");
    this->RecordProperty("Description",
                         "The ProxyEvent/ProxyField Subscribe trace points are not traced if the service element is "
                         "disabled in the Trace FilterConfig.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_subscribe = false;

    const std::size_t max_sample_count{10U};

    // Expecting that the runtime returns a mocked TracingFilterConfig
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that GetTracingRuntime() is never called
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).Times(0);

    // and that a ProxyEvent binding is created with the Subscribe trace point disabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to Subscribe should never be called

    // and that GetSubscriptionState is called once and indicates that the Service Element is currently not
    // subscribed
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kNotSubscribed));

    // and that Subscribe will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, Subscribe(max_sample_count));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and subscribe is called on the event
    this->proxy_->my_service_element_.Subscribe(max_sample_count);
}

TYPED_TEST(ProxyEventTracingUnsubscribeFixture, UnsubscribeCallsAreTracedWhenEnabled)
{
    this->RecordProperty("Verifies", "SCR-18216878, SCR-18221771, SCR-18228095");
    this->RecordProperty("Description",
                         "The Trace point types for ProxyEvent/ProxyField Unsubscribe are correctly mapped "
                         "(SCR-18216878). The Unsubscribe "
                         "trace points are traced without a LocalDataChunkList (SCR-18221771, SCR-18228095).");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_unsubscribe = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    StrictMock<tracing::TracingRuntimeMock> tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the Unsubscribe trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to Unsubscribe should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::UNSUBSCRIBE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(ResultBlank{}));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that GetSubscriptionState is called once and indicates that the Service Element is currently subscribed
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscribed));

    // and that Unsubscribe will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, Unsubscribe());

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and unsubscribe is called on the event
    this->proxy_->my_service_element_.Unsubscribe();
}

TYPED_TEST(ProxyEventTracingUnsubscribeFixture,
           UnsubscribeTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "The Trace point for ProxyEvent/ProxyField Unsubscribe should be disabled after receiving a "
                         "disable trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_unsubscribe = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the Unsubscribe trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to Unsubscribe should be called containing the correct max_sample_count which returns
    // an error
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::UNSUBSCRIBE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that GetSubscriptionState is called once and indicates that the Service Element is currently subscribed
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscribed));

    // and that Unsubscribe will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, Unsubscribe());

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and unsubscribe is called on the event
    this->proxy_->my_service_element_.Unsubscribe();

    // Then the specific trace point instance should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_unsubscribe = false;
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingUnsubscribeFixture,
           UnsubscribeTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "All Trace points for the ProxyEvent/ProxyField should be disabled after receiving a disable "
                         "all trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_unsubscribe = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the Unsubscribe trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to Unsubscribe should be called containing the correct max_sample_count which returns
    // an error
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::UNSUBSCRIBE};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that GetSubscriptionState is called once and indicates that the Service Element is currently subscribed
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscribed));

    // and that Unsubscribe will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, Unsubscribe());

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and unsubscribe is called on the event
    this->proxy_->my_service_element_.Unsubscribe();

    // Then all trace point instances should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    const ProxyEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingUnsubscribeFixture, UnsubscribeCallsAreNotTracedWhenDisabled)
{
    this->RecordProperty("Verifies", "SCR-18217128");
    this->RecordProperty("Description",
                         "The ProxyEvent/ProxyField Unsubscribe trace points are not traced if the service element is "
                         "disabled in the Trace FilterConfig.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_unsubscribe = false;

    // Expecting that the runtime returns a mocked TracingFilterConfig
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that GetTracingRuntime() is never called
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).Times(0);

    // and that a ProxyEvent binding is created with the Unsubscribe trace point disabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to Unsubscribe should never be called

    // and that GetSubscriptionState is called once and indicates that the Service Element is currently subscribed
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetSubscriptionState())
        .WillOnce(Return(SubscriptionState::kSubscribed));

    // and that Unsubscribe will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, Unsubscribe());

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and unsubscribe is called on the event
    this->proxy_->my_service_element_.Unsubscribe();
}

TYPED_TEST(ProxyEventTracingSetReceiveHandlerFixture, SetReceiveHandlerCallsAreTracedWhenEnabled)
{
    this->RecordProperty("Verifies", "SCR-18216878, SCR-18221771, SCR-18228095");
    this->RecordProperty("Description",
                         "The Trace point types for ProxyEvent/ProxyField SetReceiveHandler are correctly mapped "
                         "(SCR-18216878). The SetReceiveHandler trace points are traced without a LocalDataChunkList "
                         "(SCR-18221771, SCR-18228095).");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_set_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    StrictMock<tracing::TracingRuntimeMock> tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the SetReceiveHandler trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to SetReceiveHandler should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::SET_RECEIVE_HANDLER};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(ResultBlank{}));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that SetReceiveHandler will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, SetReceiveHandler(_));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    this->proxy_->my_service_element_.SetReceiveHandler(EventReceiveHandler{});
}

TYPED_TEST(ProxyEventTracingSetReceiveHandlerFixture,
           SetReceiveHandlerTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty(
        "Description",
        "The Trace point for ProxyEvent/ProxyField SetReceiveHandler should be disabled after receiving a "
        "disable trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_set_receive_handler = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the SetReceiveHandler trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to SetReceiveHandler should be called which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::SET_RECEIVE_HANDLER};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that SetReceiveHandler will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, SetReceiveHandler(_));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    this->proxy_->my_service_element_.SetReceiveHandler(EventReceiveHandler{});

    // Then the specific trace point instance should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_set_receive_handler = false;
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingSetReceiveHandlerFixture,
           SetReceiveHandlerTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "All Trace points for the ProxyEvent/ProxyField should be disabled after receiving a disable "
                         "all trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_set_receive_handler = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the SetReceiveHandler trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to SetReceiveHandler should be called which returns an error
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::SET_RECEIVE_HANDLER};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that SetReceiveHandler will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, SetReceiveHandler(_));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    this->proxy_->my_service_element_.SetReceiveHandler(EventReceiveHandler{});

    // Then all trace point instances should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    const ProxyEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingSetReceiveHandlerFixture, SetReceiveHandlerCallsAreNotTracedWhenDisabled)
{
    this->RecordProperty("Verifies", "SCR-18217128");
    this->RecordProperty(
        "Description",
        "The ProxyEvent/ProxyField SetReceiveHandler trace points are not traced if the service element is "
        "disabled in the Trace FilterConfig.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_set_receive_handler = false;

    // Expecting that the runtime returns a mocked TracingFilterConfig
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that GetTracingRuntime() is never called
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).Times(0);

    // and that a ProxyEvent binding is created with the SetReceiveHandler trace point disabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to SetReceiveHandler should never be called

    // and that SetReceiveHandler will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, SetReceiveHandler(_));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    this->proxy_->my_service_element_.SetReceiveHandler(EventReceiveHandler{});
}

TYPED_TEST(ProxyEventTracingReceiveHandlerCallbackFixture, ReceiveHandlerCallbackCallsAreTracedWhenEnabled)
{
    this->RecordProperty("Verifies", "SCR-18216878, SCR-18221771, SCR-18228095");
    this->RecordProperty("Description",
                         "The Trace point types for ProxyEvent/ProxyField ReceiveHandlerCallback are correctly mapped "
                         "(SCR-18216878). The ReceiveHandlerCallback trace points are traced without a "
                         "LocalDataChunkList (SCR-18221771, SCR-18228095).");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    StrictMock<tracing::TracingRuntimeMock> tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the ReceiveHandlerCallback trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // and that SetReceiveHandler will be registered with the binding with the wrapped handler containing the trace call
    std::weak_ptr<ScopedEventReceiveHandler> scoped_event_receive_handler_weak_ptr{};
    EXPECT_CALL(*this->mock_proxy_event_binding_, SetReceiveHandler(_))
        .WillOnce(Invoke(
            [&scoped_event_receive_handler_weak_ptr](std::weak_ptr<ScopedEventReceiveHandler> handler) -> ResultBlank {
                scoped_event_receive_handler_weak_ptr = std::move(handler);
                return {};
            }));

    // Then a trace call relating to ReceiveHandlerCallback should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::RECEIVE_HANDLER_CALLBACK};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    EventReceiveHandler handler = []() {};
    this->proxy_->my_service_element_.SetReceiveHandler(std::move(handler));

    // and the wrapped handler is called
    auto scoped_event_receive_handler_shared_ptr = scoped_event_receive_handler_weak_ptr.lock();
    ASSERT_TRUE(scoped_event_receive_handler_shared_ptr);
    (*scoped_event_receive_handler_shared_ptr)();
}

TYPED_TEST(ProxyEventTracingReceiveHandlerCallbackFixture,
           ReceiveHandlerCallbackTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty(
        "Description",
        "The Trace point for ProxyEvent/ProxyField ReceiveHandlerCallback should be disabled after receiving a "
        "disable trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_call_receive_handler = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the ReceiveHandlerCallback trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // and that SetReceiveHandler will be registered with the binding with the wrapped handler containing the trace call
    std::weak_ptr<ScopedEventReceiveHandler> scoped_event_receive_handler_weak_ptr{};
    EXPECT_CALL(*this->mock_proxy_event_binding_, SetReceiveHandler(_))
        .WillOnce(Invoke(
            [&scoped_event_receive_handler_weak_ptr](std::weak_ptr<ScopedEventReceiveHandler> handler) -> ResultBlank {
                scoped_event_receive_handler_weak_ptr = std::move(handler);
                return {};
            }));

    // Then a trace call relating to ReceiveHandlerCallback should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::RECEIVE_HANDLER_CALLBACK};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    EventReceiveHandler handler = []() {};
    this->proxy_->my_service_element_.SetReceiveHandler(std::move(handler));

    // and the wrapped handler is called
    auto scoped_event_receive_handler_shared_ptr = scoped_event_receive_handler_weak_ptr.lock();
    ASSERT_TRUE(scoped_event_receive_handler_shared_ptr);
    (*scoped_event_receive_handler_shared_ptr)();

    // Then the specific trace point instance should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_call_receive_handler = false;
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingReceiveHandlerCallbackFixture,
           ReceiveHandlerCallbackTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "All Trace points for the ProxyEvent/ProxyField should be disabled after receiving a disable "
                         "all trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_call_receive_handler = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the ReceiveHandlerCallback trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // and that SetReceiveHandler will be registered with the binding with the wrapped handler containing the trace call
    std::weak_ptr<ScopedEventReceiveHandler> scoped_event_receive_handler_weak_ptr{};
    EXPECT_CALL(*this->mock_proxy_event_binding_, SetReceiveHandler(_))
        .WillOnce(Invoke(
            [&scoped_event_receive_handler_weak_ptr](std::weak_ptr<ScopedEventReceiveHandler> handler) -> ResultBlank {
                scoped_event_receive_handler_weak_ptr = std::move(handler);
                return {};
            }));

    // Then a trace call relating to ReceiveHandlerCallback should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::RECEIVE_HANDLER_CALLBACK};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    EventReceiveHandler handler = []() {};
    this->proxy_->my_service_element_.SetReceiveHandler(std::move(handler));

    // and the wrapped handler is called
    auto scoped_event_receive_handler_shared_ptr = scoped_event_receive_handler_weak_ptr.lock();
    ASSERT_TRUE(scoped_event_receive_handler_shared_ptr);
    (*scoped_event_receive_handler_shared_ptr)();

    // Then all trace point instances should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    const ProxyEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingReceiveHandlerCallbackFixture, ReceiveHandlerCallbackCallsAreNotTracedWhenDisabled)
{
    this->RecordProperty("Verifies", "SCR-18217128");
    this->RecordProperty(
        "Description",
        "The ProxyEvent/ProxyField ReceiveHandlerCallback trace points are not traced if the service element is "
        "disabled in the Trace FilterConfig.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_call_receive_handler = false;

    // Expecting that the runtime returns a mocked TracingFilterConfig
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that GetTracingRuntime() is never called
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).Times(0);

    // and that a ProxyEvent binding is created with the ReceiveHandlerCallback trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // and that SetReceiveHandler will be registered with the binding with the wrapped handler containing the trace call
    std::weak_ptr<ScopedEventReceiveHandler> scoped_event_receive_handler_weak_ptr{};
    EXPECT_CALL(*this->mock_proxy_event_binding_, SetReceiveHandler(_))
        .WillOnce(Invoke(
            [&scoped_event_receive_handler_weak_ptr](std::weak_ptr<ScopedEventReceiveHandler> handler) -> ResultBlank {
                scoped_event_receive_handler_weak_ptr = std::move(handler);
                return {};
            }));

    // Then a trace call relating to ReceiveHandlerCallback should never be called

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    EventReceiveHandler handler = []() {};
    this->proxy_->my_service_element_.SetReceiveHandler(std::move(handler));

    // and the wrapped handler is called
    auto scoped_event_receive_handler_shared_ptr = scoped_event_receive_handler_weak_ptr.lock();
    ASSERT_TRUE(scoped_event_receive_handler_shared_ptr);
    (*scoped_event_receive_handler_shared_ptr)();
}

TYPED_TEST(ProxyEventTracingUnsetReceiveHandlerFixture, UnsetReceiveHandlerCallsAreTracedWhenEnabled)
{
    this->RecordProperty("Verifies", "SCR-18216878, SCR-18221771, SCR-18228095");
    this->RecordProperty("Description",
                         "The Trace point types for ProxyEvent/ProxyField UnsetReceiveHandler are correctly mapped "
                         "(SCR-18216878). The UnsetReceiveHandler trace points are traced without a LocalDataChunkList "
                         "(SCR-18221771, SCR-18228095).");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_unset_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    StrictMock<tracing::TracingRuntimeMock> tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the UnsetReceiveHandler trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to UnsetReceiveHandler should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::UNSET_RECEIVE_HANDLER};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that UnsetReceiveHandler will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, UnsetReceiveHandler());

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    this->proxy_->my_service_element_.SetReceiveHandler(EventReceiveHandler{});

    // and UnsetReceiveHandler is called on the event
    this->proxy_->my_service_element_.UnsetReceiveHandler();
}

TYPED_TEST(ProxyEventTracingUnsetReceiveHandlerFixture,
           UnsetReceiveHandlerTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty(
        "Description",
        "The Trace point for ProxyEvent/ProxyField UnsetReceiveHandler should be disabled after receiving a "
        "disable trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_unset_receive_handler = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the UnsetReceiveHandler trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to UnsetReceiveHandler should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::UNSET_RECEIVE_HANDLER};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that UnsetReceiveHandler will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, UnsetReceiveHandler());

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    this->proxy_->my_service_element_.SetReceiveHandler(EventReceiveHandler{});

    // and UnsetReceiveHandler is called on the event
    this->proxy_->my_service_element_.UnsetReceiveHandler();

    // Then the specific trace point instance should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_unset_receive_handler = false;
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingUnsetReceiveHandlerFixture,
           UnsetReceiveHandlerTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "All Trace points for the ProxyEvent/ProxyField should be disabled after receiving a disable "
                         "all trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_unset_receive_handler = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the UnsetReceiveHandler trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to UnsetReceiveHandler should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::UNSET_RECEIVE_HANDLER};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // and that UnsetReceiveHandler will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, UnsetReceiveHandler());

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    this->proxy_->my_service_element_.SetReceiveHandler(EventReceiveHandler{});

    // and UnsetReceiveHandler is called on the event
    this->proxy_->my_service_element_.UnsetReceiveHandler();

    // Then all trace point instances should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    const ProxyEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingUnsetReceiveHandlerFixture, UnsetReceiveHandlerCallsAreNotTracedWhenDisabled)
{
    this->RecordProperty("Verifies", "SCR-18217128");
    this->RecordProperty(
        "Description",
        "The ProxyEvent/ProxyField UnsetReceiveHandler trace points are not traced if the service element is "
        "disabled in the Trace FilterConfig.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_unset_receive_handler = false;

    // Expecting that the runtime returns a mocked TracingFilterConfig
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that GetTracingRuntime() is never called
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).Times(0);

    // and that a ProxyEvent binding is created with the UnsetReceiveHandler trace point disabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to UnsetReceiveHandler should never be called

    // and that UnsetReceiveHandler will be called on the binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, UnsetReceiveHandler());

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and SetReceiveHandler is called on the event
    this->proxy_->my_service_element_.SetReceiveHandler(EventReceiveHandler{});

    // and unsubscribe is called on the event
    this->proxy_->my_service_element_.UnsetReceiveHandler();
}

TYPED_TEST(ProxyEventTracingGetNewSamplesFixture, GetNewSamplesCallsAreTracedWhenEnabled)
{
    this->RecordProperty("Verifies", "SCR-18216878, SCR-18221771, SCR-18228095");
    this->RecordProperty("Description",
                         "The Trace point types for ProxyEvent/ProxyField GetNewSamples are correctly mapped "
                         "(SCR-18216878). The GetNewSamples trace points are traced without a LocalDataChunkList "
                         "(SCR-18221771, SCR-18228095).");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_get_new_samples = true;

    const std::size_t max_num_samples{1U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    StrictMock<tracing::TracingRuntimeMock> tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the GetNewSamples trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to GetNewSamples should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::GET_NEW_SAMPLES};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and GetNewSamples is called on the event
    this->proxy_->my_service_element_.GetNewSamples([](SamplePtr<TestSampleType>) {}, max_num_samples);
}

TYPED_TEST(ProxyEventTracingGetNewSamplesFixture,
           GetNewSamplesTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "The Trace point for ProxyEvent/ProxyField GetNewSamples should be disabled after receiving a "
                         "disable trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_get_new_samples = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const std::size_t max_num_samples{1U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the GetNewSamples trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to GetNewSamples should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::GET_NEW_SAMPLES};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and GetNewSamples is called on the event
    this->proxy_->my_service_element_.GetNewSamples([](SamplePtr<TestSampleType>) {}, max_num_samples);

    // Then the specific trace point instance should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_get_new_samples = false;
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingGetNewSamplesFixture,
           GetNewSamplesTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "All Trace points for the ProxyEvent/ProxyField should be disabled after receiving a disable "
                         "all trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_get_new_samples = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const std::size_t max_num_samples{1U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    tracing::TracingRuntimeMock tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the GetNewSamples trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to GetNewSamples should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::GET_NEW_SAMPLES};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      score::cpp::optional<tracing::ITracingRuntime::TracePointDataId>{},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and GetNewSamples is called on the event
    this->proxy_->my_service_element_.GetNewSamples([](SamplePtr<TestSampleType>) {}, max_num_samples);

    // Then all trace point instances should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    const ProxyEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingGetNewSamplesFixture, GetNewSamplesCallsAreNotTracedWhenDisabled)
{
    this->RecordProperty("Verifies", "SCR-18217128");
    this->RecordProperty(
        "Description",
        "The ProxyEvent/ProxyField GetNewSamples trace points are not traced if the service element is "
        "disabled in the Trace FilterConfig.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_get_new_samples = false;

    const std::size_t max_num_samples{1U};

    // Expecting that the runtime returns a mocked TracingFilterConfig
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that GetTracingRuntime() is never called
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).Times(0);

    // and that a ProxyEvent binding is created with the GetNewSamples trace point disabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // Then a trace call relating to GetNewSamples should never be called

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and unsubscribe is called on the event
    this->proxy_->my_service_element_.GetNewSamples([](SamplePtr<TestSampleType>) {}, max_num_samples);
}

TYPED_TEST(ProxyEventTracingGetNewSamplesCallbackFixture, GetNewSamplesCallbackCallsAreTracedWhenEnabled)
{
    this->RecordProperty("Verifies", "SCR-18216878, SCR-18221771, SCR-18228095, SCR-18200787");
    this->RecordProperty(
        "Description",
        "The Trace point types for ProxyEvent/ProxyField GetNewSamplesCallback are correctly mapped "
        "(SCR-18216878). The GetNewSamplesCallback trace points are traced without a "
        "LocalDataChunkList (SCR-18221771, SCR-18228095). The GetNewSamplesCallback trace points are traced with a "
        "trace_point_data_id.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_new_samples_callback = true;

    const std::size_t max_num_samples{1U};
    const tracing::ITracingRuntime::TracePointDataId timestamp{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    StrictMock<tracing::TracingRuntimeMock> tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the GetNewSamplesCallback trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // and that GetNewSamples will be called on the binding with the wrapped handler containing the trace call
    typename ProxyEventBinding<TestSampleType>::Callback wrapper_get_new_samples_callback{};
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetNewSamples(_, _))
        .WillOnce(WithArgs<0>(
            Invoke([&wrapper_get_new_samples_callback](
                       typename ProxyEventBinding<TestSampleType>::Callback&& callback) -> Result<std::size_t> {
                wrapper_get_new_samples_callback = std::move(callback);
                return 1U;
            })));

    // Then a trace call relating to GetNewSamplesCallback should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::GET_NEW_SAMPLES_CALLBACK};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      {timestamp},
                      nullptr,
                      0U));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and the SampleReferenceTracker within the ProxyEvent has sufficient samples available
    auto& tracker = ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetSampleReferenceTracker();
    tracker.Reset(1);

    // and GetNewSamplesCallback is called on the event
    score::cpp::callback<void(SamplePtr<TestSampleType>)> callback = [](SamplePtr<TestSampleType>) noexcept {};
    this->proxy_->my_service_element_.GetNewSamples(std::move(callback), max_num_samples);

    // and the wrapped handler is called
    wrapper_get_new_samples_callback(SamplePtr<TestSampleType>{}, timestamp);
}

TYPED_TEST(ProxyEventTracingGetNewSamplesCallbackFixture,
           GetNewSamplesCallbackTracePointShouldBeDisabledAfterTraceReturnsDisableTracePointError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty(
        "Description",
        "The Trace point for ProxyEvent/ProxyField GetNewSamplesCallback should be disabled after receiving a "
        "disable trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_new_samples_callback = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const std::size_t max_num_samples{1U};
    const tracing::ITracingRuntime::TracePointDataId timestamp{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    StrictMock<tracing::TracingRuntimeMock> tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the GetNewSamplesCallback trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // and that GetNewSamples will be called on the binding with the wrapped handler containing the trace call
    typename ProxyEventBinding<TestSampleType>::Callback wrapper_get_new_samples_callback{};
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetNewSamples(_, _))
        .WillOnce(WithArgs<0>(
            Invoke([&wrapper_get_new_samples_callback](
                       typename ProxyEventBinding<TestSampleType>::Callback&& callback) -> Result<std::size_t> {
                wrapper_get_new_samples_callback = std::move(callback);
                return 1U;
            })));

    // Then a trace call relating to GetNewSamplesCallback should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::GET_NEW_SAMPLES_CALLBACK};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      {timestamp},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and the SampleReferenceTracker within the ProxyEvent has sufficient samples available
    auto& tracker = ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetSampleReferenceTracker();
    tracker.Reset(1);

    // and GetNewSamplesCallback is called on the event
    score::cpp::callback<void(SamplePtr<TestSampleType>)> callback = [](SamplePtr<TestSampleType>) noexcept {};
    this->proxy_->my_service_element_.GetNewSamples(std::move(callback), max_num_samples);

    // and the wrapped handler is called
    wrapper_get_new_samples_callback(SamplePtr<TestSampleType>{}, timestamp);

    // Then the specific trace point instance should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    auto expected_enabled_trace_points_after_error = expected_enabled_trace_points;
    expected_enabled_trace_points_after_error.enable_new_samples_callback = false;
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingGetNewSamplesCallbackFixture,
           GetNewSamplesCallbackTracePointShouldBeDisabledAfterTraceReturnsDisableAllTracePointsError)
{
    this->RecordProperty("Verifies", "SCR-18398059");
    this->RecordProperty("Description",
                         "All Trace points for the ProxyEvent/ProxyField should be disabled after receiving a disable "
                         "all trace point error from the tracing runtime Trace call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_new_samples_callback = true;
    expected_enabled_trace_points.enable_subscribe = true;
    expected_enabled_trace_points.enable_call_receive_handler = true;

    const std::size_t max_num_samples{1U};
    const tracing::ITracingRuntime::TracePointDataId timestamp{10U};

    const tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view =
        this->CreateServiceElementInstanceIdentifierView(
            typename ProxyEventTracingFixture<TypeParam>::TracePointType{});

    // Expecting that the runtime returns a mocked TracingRuntime and TracingFilterConfig
    StrictMock<tracing::TracingRuntimeMock> tracing_runtime_mock{};
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillOnce(Return(&tracing_runtime_mock));
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that a ProxyEvent binding is created with the GetNewSamplesCallback trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // and that GetNewSamples will be called on the binding with the wrapped handler containing the trace call
    typename ProxyEventBinding<TestSampleType>::Callback wrapper_get_new_samples_callback{};
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetNewSamples(_, _))
        .WillOnce(WithArgs<0>(
            Invoke([&wrapper_get_new_samples_callback](
                       typename ProxyEventBinding<TestSampleType>::Callback&& callback) -> Result<std::size_t> {
                wrapper_get_new_samples_callback = std::move(callback);
                return 1U;
            })));

    // Then a trace call relating to GetNewSamplesCallback should be called with no data
    tracing::ITracingRuntime::TracePointType trace_point_type{
        ProxyEventTracingFixture<TypeParam>::TracePointType::GET_NEW_SAMPLES_CALLBACK};
    EXPECT_CALL(tracing_runtime_mock,
                Trace(BindingType::kLoLa,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      {timestamp},
                      nullptr,
                      0U))
        .WillOnce(Return(MakeUnexpected(tracing::TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // and that GetBindingType is called on the proxy event binding
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetBindingType()).WillOnce(Return(BindingType::kLoLa));

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and the SampleReferenceTracker within the ProxyEvent has sufficient samples available
    auto& tracker = ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetSampleReferenceTracker();
    tracker.Reset(1);

    // and GetNewSamplesCallback is called on the event
    score::cpp::callback<void(SamplePtr<TestSampleType>)> callback = [](SamplePtr<TestSampleType>) noexcept {};
    this->proxy_->my_service_element_.GetNewSamples(std::move(callback), max_num_samples);

    // and the wrapped handler is called
    wrapper_get_new_samples_callback(SamplePtr<TestSampleType>{}, timestamp);

    // Then all trace point instances should now be disabled
    const ProxyEventTracingData actual_enabled_trace_points =
        ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetProxyEventTracing();

    const ProxyEventTracingData expected_enabled_trace_points_after_error{};
    EXPECT_TRUE(this->AreTracePointsEqual(actual_enabled_trace_points, expected_enabled_trace_points_after_error));
}

TYPED_TEST(ProxyEventTracingGetNewSamplesCallbackFixture, GetNewSamplesCallbackCallsAreNotTracedWhenDisabled)
{
    this->RecordProperty("Verifies", "SCR-18217128");
    this->RecordProperty(
        "Description",
        "The ProxyEvent/ProxyField GetNewSamplesCallback trace points are not traced if the service element is "
        "disabled in the Trace FilterConfig.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    ProxyEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_new_samples_callback = false;

    const std::size_t max_num_samples{1U};
    const tracing::ITracingRuntime::TracePointDataId timestamp{10U};

    // Expecting that the runtime returns a mocked TracingFilterConfig
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig())
        .WillOnce(Return(&(this->tracing_filter_config_mock_)));

    // and that GetTracingRuntime() is never called
    EXPECT_CALL(this->runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).Times(0);

    // and that a ProxyEvent binding is created with the GetNewSamplesCallback trace point enabled.
    ProxyEventTracingFixture<TypeParam>::template ExpectIsTracePointEnabledCalls<
        typename ProxyEventTracingFixture<TypeParam>::TracePointType>(expected_enabled_trace_points,
                                                                      kServiceIdentifierStringView,
                                                                      score::cpp::string_view{kServiceElementName},
                                                                      kInstanceSpecifierStringView);

    // and that GetNewSamples will be called on the binding with the wrapped handler containing the trace call
    typename ProxyEventBinding<TestSampleType>::Callback wrapper_get_new_samples_callback{};
    EXPECT_CALL(*this->mock_proxy_event_binding_, GetNewSamples(_, _))
        .WillOnce(WithArgs<0>(
            Invoke([&wrapper_get_new_samples_callback](
                       typename ProxyEventBinding<TestSampleType>::Callback&& callback) -> Result<std::size_t> {
                wrapper_get_new_samples_callback = std::move(callback);
                return 1U;
            })));

    // Then a trace call relating to GetNewSamplesCallback should never be called

    // When a Proxy containing a ProxyEvent is created based on a lola deployment
    this->CreateProxy();

    // and the SampleReferenceTracker within the ProxyEvent has sufficient samples available
    auto& tracker = ProxyEventBaseAttorney{this->proxy_->my_service_element_}.GetSampleReferenceTracker();
    tracker.Reset(1);

    // and GetNewSamplesCallback is called on the event
    score::cpp::callback<void(SamplePtr<TestSampleType>)> callback = [](SamplePtr<TestSampleType>) noexcept {};
    this->proxy_->my_service_element_.GetNewSamples(std::move(callback), max_num_samples);

    // and the wrapped handler is called
    wrapper_get_new_samples_callback(SamplePtr<TestSampleType>{}, timestamp);
}

}  // namespace
}  // namespace score::mw::com::impl
