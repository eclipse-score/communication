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
#include "score/mw/com/impl/tracing/proxy_event_tracing.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"
#include "score/mw/com/impl/tracing/proxy_event_tracing_data.h"
#include "score/mw/com/impl/tracing/trace_error.h"
#include "score/mw/com/impl/tracing/tracing_runtime_mock.h"
#include "score/result/result.h"

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace score::mw::com::impl::tracing
{
namespace
{

using namespace ::testing;

class ProxyEventTracingFixture : public ::testing::TestWithParam<ServiceElementType>
{
  public:
    ProxyEventTracingFixture()
    {
        ON_CALL(proxy_event_binding_base_, GetBindingType()).WillByDefault(Return(BindingType::kFake));
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(&tracing_runtime_mock_));
    }

    ProxyEventTracingFixture& WithAProxyEventTracingDataWithInvalidElementType()
    {
        proxy_event_tracing_data_.service_element_instance_identifier_view.service_element_identifier_view
            .service_element_type = static_cast<ServiceElementType>(100U);
        return *this;
    }

    ProxyEventTracingFixture& WithAValidProxyEventTracingData()
    {
        proxy_event_tracing_data_.service_element_instance_identifier_view.service_element_identifier_view
            .service_element_type = GetParam();
        return *this;
    }

    ProxyEventTracingFixture& WithAllTracePointsEnabled()
    {
        proxy_event_tracing_data_.enable_subscribe = true;
        proxy_event_tracing_data_.enable_unsubscribe = true;
        proxy_event_tracing_data_.enable_subscription_state_changed = true;
        proxy_event_tracing_data_.enable_set_subcription_state_change_handler = true;
        proxy_event_tracing_data_.enable_unset_subscription_state_change_handler = true;
        proxy_event_tracing_data_.enable_call_subscription_state_change_handler = true;
        proxy_event_tracing_data_.enable_set_receive_handler = true;
        proxy_event_tracing_data_.enable_unset_receive_handler = true;
        proxy_event_tracing_data_.enable_call_receive_handler = true;
        proxy_event_tracing_data_.enable_get_new_samples = true;
        proxy_event_tracing_data_.enable_new_samples_callback = true;

        return *this;
    }

    bool AreAllTracePointsDisabled(const ProxyEventTracingData& proxy_event_tracing_data)
    {
        return (!proxy_event_tracing_data.enable_subscribe && !proxy_event_tracing_data.enable_unsubscribe &&
                !proxy_event_tracing_data.enable_subscription_state_changed &&
                !proxy_event_tracing_data.enable_set_subcription_state_change_handler &&
                !proxy_event_tracing_data.enable_unset_subscription_state_change_handler &&
                !proxy_event_tracing_data.enable_call_subscription_state_change_handler &&
                !proxy_event_tracing_data.enable_set_receive_handler &&
                !proxy_event_tracing_data.enable_unset_receive_handler &&
                !proxy_event_tracing_data.enable_call_receive_handler &&
                !proxy_event_tracing_data.enable_get_new_samples &&
                !proxy_event_tracing_data.enable_new_samples_callback);
    }

    using TestSampleType = std::uint32_t;

    ProxyEventTracingData proxy_event_tracing_data_{};
    mock_binding::ProxyEvent<TestSampleType> proxy_event_binding_base_{};

    TracingRuntimeMock tracing_runtime_mock_{};
    RuntimeMockGuard runtime_mock_guard_{};
};

using ProxyEventTraceSubscribeFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceSubscribeFixture,
                         ProxyEventTraceSubscribeFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using ProxyEventTraceUnsubscribeFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceUnsubscribeFixture,
                         ProxyEventTraceUnsubscribeFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using ProxyEventTraceSetReceiveHandlerFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceSetReceiveHandlerFixture,
                         ProxyEventTraceSetReceiveHandlerFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using ProxyEventTraceUnsetReceiveHandlerFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceUnsetReceiveHandlerFixture,
                         ProxyEventTraceUnsetReceiveHandlerFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using ProxyEventTraceGetNewSamplesFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceGetNewSamplesFixture,
                         ProxyEventTraceGetNewSamplesFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using ProxyEventTraceCallGetNewSamplesCallbackFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceCallGetNewSamplesCallbackFixture,
                         ProxyEventTraceCallGetNewSamplesCallbackFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using ProxyEventTraceCallReceiveHandlerFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceCallReceiveHandlerFixture,
                         ProxyEventTraceCallReceiveHandlerFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

using ProxyEventCreateTracingGetNewSamplesCallbackFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventCreateTracingGetNewSamplesCallbackFixture,
                         ProxyEventCreateTracingGetNewSamplesCallbackFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(ProxyEventTraceSubscribeFixture, TraceSubscribeWillDispatchToBindingTracingRuntime)
{
    const std::size_t max_sample_count{1U};

    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceSubscribe
    TraceSubscribe(proxy_event_tracing_data_, proxy_event_binding_base_, max_sample_count);
}

TEST_P(ProxyEventTraceSubscribeFixture, TraceSubscribeWillNotDispatchToBindingTracingRuntimeIfTracingDisabled)
{
    const std::size_t max_sample_count{1U};

    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceSubscribe
    TraceSubscribe(proxy_event_tracing_data_, proxy_event_binding_base_, max_sample_count);
}

TEST_P(ProxyEventTraceSubscribeFixture,
       TraceSubscribeWillDisableTracePointIfDisableInstanceErrorReturnedFromBindingTracingRuntime)
{
    const std::size_t max_sample_count{1U};

    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable trace point instance
    // error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceSubscribe
    TraceSubscribe(proxy_event_tracing_data_, proxy_event_binding_base_, max_sample_count);

    // Then the enable_subscribe trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_subscribe);
}

TEST_P(ProxyEventTraceSubscribeFixture,
       TraceSubscribeWillDisableTracePointIfDisableAllTracePointsErrorReturnedFromBindingTracingRuntime)
{
    const std::size_t max_sample_count{1U};

    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable all trace points error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceSubscribe
    TraceSubscribe(proxy_event_tracing_data_, proxy_event_binding_base_, max_sample_count);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

TEST_P(ProxyEventTraceSubscribeFixture, TraceSubscribeWillIgnoreUnknownErrorFromBindingTracingRuntime)
{
    const std::size_t max_sample_count{1U};

    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns an unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceSubscribe
    TraceSubscribe(proxy_event_tracing_data_, proxy_event_binding_base_, max_sample_count);

    // Then the enable_subscribe trace point is still enabled
    EXPECT_TRUE(proxy_event_tracing_data_.enable_subscribe);
}

using ProxyEventTraceSubscribeDeathTest = ProxyEventTracingFixture;
TEST_F(ProxyEventTraceSubscribeDeathTest, TraceSubscribeWithInvalidTraceServiceElementTypeTerminates)
{
    const std::size_t max_sample_count{1U};

    // Given a ProxyEventTracingData with an invalid element type and all trace points enabled
    WithAProxyEventTracingDataWithInvalidElementType().WithAllTracePointsEnabled();

    // When calling TraceSubscribe
    // Then we terminate
    EXPECT_DEATH(TraceSubscribe(proxy_event_tracing_data_, proxy_event_binding_base_, max_sample_count), ".*");
}

using ProxyEventTraceUnsubscribeFixture = ProxyEventTracingFixture;
TEST_P(ProxyEventTraceUnsubscribeFixture, TraceUnsubscribeWillDispatchToBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceUnsubscribe
    TraceUnsubscribe(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceUnsubscribeFixture, TraceSubscribeWillNotDispatchToBindingTracingRuntimeIfTracingDisabled)
{
    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceUnsubscribe
    TraceUnsubscribe(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceUnsubscribeFixture,
       TraceUnsubscribeWillDisableTracePointIfDisableInstanceErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable trace point instance
    // error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceUnsubscribe
    TraceUnsubscribe(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_unsubscribe trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_unsubscribe);
}

TEST_P(ProxyEventTraceUnsubscribeFixture,
       TraceUnsubscribeWillDisableTracePointIfDisableAllTracePointsErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable all trace points error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceUnsubscribe
    TraceUnsubscribe(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

TEST_P(ProxyEventTraceUnsubscribeFixture, TraceUnsubscribeWillIgnoreUnknownErrorFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns an unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceUnsubscribe
    TraceUnsubscribe(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_unsubscribe trace point is still enabled
    EXPECT_TRUE(proxy_event_tracing_data_.enable_unsubscribe);
}

using ProxyEventTraceUnsubscribeDeathTest = ProxyEventTracingFixture;
TEST_F(ProxyEventTraceUnsubscribeDeathTest, TraceUnsubscribeWithInvalidTraceServiceElementTypeTerminates)
{
    // Given a ProxyEventTracingData with an invalid element type and all trace points enabled
    WithAProxyEventTracingDataWithInvalidElementType().WithAllTracePointsEnabled();

    // When calling TraceUnsubscribe
    // Then we terminate
    EXPECT_DEATH(TraceUnsubscribe(proxy_event_tracing_data_, proxy_event_binding_base_), ".*");
}

using ProxyEventTraceSetReceiveHandlerFixture = ProxyEventTracingFixture;
TEST_P(ProxyEventTraceSetReceiveHandlerFixture, TraceSetReceiveHandlerWillDispatchToBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceSetReceiveHandler
    TraceSetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceSetReceiveHandlerFixture, TraceSubscribeWillNotDispatchToBindingTracingRuntimeIfTracingDisabled)
{
    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceSetReceiveHandler
    TraceSetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceSetReceiveHandlerFixture,
       TraceSetReceiveHandlerWillDisableTracePointIfDisableInstanceErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable trace point instance
    // error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceSetReceiveHandler
    TraceSetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_set_receive_handler trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_set_receive_handler);
}

TEST_P(ProxyEventTraceSetReceiveHandlerFixture,
       TraceSetReceiveHandlerWillDisableTracePointIfDisableAllTracePointsErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable all trace points error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceSetReceiveHandler
    TraceSetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

TEST_P(ProxyEventTraceSetReceiveHandlerFixture, TraceSetReceiveHandlerWillIgnoreUnknownErrorFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns an unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceSetReceiveHandler
    TraceSetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_set_receive_handler trace point is still enabled
    EXPECT_TRUE(proxy_event_tracing_data_.enable_set_receive_handler);
}

using ProxyEventTraceSetReceiveHandlerDeathTest = ProxyEventTracingFixture;
TEST_F(ProxyEventTraceSetReceiveHandlerDeathTest, TraceSetReceiveHandlerWithInvalidTraceServiceElementTypeTerminates)
{
    // Given a ProxyEventTracingData with an invalid element type and all trace points enabled
    WithAProxyEventTracingDataWithInvalidElementType().WithAllTracePointsEnabled();

    // When calling TraceSetReceiveHandler
    // Then we terminate
    EXPECT_DEATH(TraceSetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_), ".*");
}

using ProxyEventTraceUnsetReceiveHandlerFixture = ProxyEventTracingFixture;
TEST_P(ProxyEventTraceUnsetReceiveHandlerFixture, TraceUnsetReceiveHandlerWillDispatchToBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceUnsetReceiveHandler
    TraceUnsetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceUnsetReceiveHandlerFixture, TraceSubscribeWillNotDispatchToBindingTracingRuntimeIfTracingDisabled)
{
    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceUnsetReceiveHandler
    TraceUnsetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceUnsetReceiveHandlerFixture,
       TraceUnsetReceiveHandlerWillDisableTracePointIfDisableInstanceErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable trace point instance
    // error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceUnsetReceiveHandler
    TraceUnsetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_unset_receive_handler trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_unset_receive_handler);
}

TEST_P(ProxyEventTraceUnsetReceiveHandlerFixture,
       TraceUnsetReceiveHandlerWillDisableTracePointIfDisableAllTracePointsErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable all trace points error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceUnsetReceiveHandler
    TraceUnsetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

TEST_P(ProxyEventTraceUnsetReceiveHandlerFixture,
       TraceUnsetReceiveHandlerWillIgnoreUnknownErrorFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns an unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceUnsetReceiveHandler
    TraceUnsetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_unset_receive_handler trace point is still enabled
    EXPECT_TRUE(proxy_event_tracing_data_.enable_unset_receive_handler);
}

using ProxyEventTraceUnsetReceiveHandlerDeathTest = ProxyEventTracingFixture;
TEST_F(ProxyEventTraceUnsetReceiveHandlerDeathTest,
       TraceUnsetReceiveHandlerWithInvalidTraceServiceElementTypeTerminates)
{
    // Given a ProxyEventTracingData with an invalid element type and all trace points enabled
    WithAProxyEventTracingDataWithInvalidElementType().WithAllTracePointsEnabled();

    // When calling TraceUnsetReceiveHandler
    // Then we terminate
    EXPECT_DEATH(TraceUnsetReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_), ".*");
}

using ProxyEventTraceGetNewSamplesFixture = ProxyEventTracingFixture;
TEST_P(ProxyEventTraceGetNewSamplesFixture, TraceGetNewSamplesWillDispatchToBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceGetNewSamples
    TraceGetNewSamples(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceGetNewSamplesFixture, TraceSubscribeWillNotDispatchToBindingTracingRuntimeIfTracingDisabled)
{
    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceGetNewSamples
    TraceGetNewSamples(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceGetNewSamplesFixture,
       TraceGetNewSamplesWillDisableTracePointIfDisableInstanceErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable trace point instance
    // error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceGetNewSamples
    TraceGetNewSamples(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_get_new_samples trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_get_new_samples);
}

TEST_P(ProxyEventTraceGetNewSamplesFixture,
       TraceGetNewSamplesWillDisableTracePointIfDisableAllTracePointsErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable all trace points error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceGetNewSamples
    TraceGetNewSamples(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

TEST_P(ProxyEventTraceGetNewSamplesFixture, TraceGetNewSamplesWillIgnoreUnknownErrorFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns an unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceGetNewSamples
    TraceGetNewSamples(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_get_new_samples trace point is still enabled
    EXPECT_TRUE(proxy_event_tracing_data_.enable_get_new_samples);
}

using ProxyEventTraceGetNewSamplesDeathTest = ProxyEventTracingFixture;
TEST_F(ProxyEventTraceGetNewSamplesDeathTest, TraceGetNewSamplesWithInvalidTraceServiceElementTypeTerminates)
{
    // Given a ProxyEventTracingData with an invalid element type and all trace points enabled
    WithAProxyEventTracingDataWithInvalidElementType().WithAllTracePointsEnabled();

    // When calling TraceGetNewSamples
    // Then we terminate
    EXPECT_DEATH(TraceGetNewSamples(proxy_event_tracing_data_, proxy_event_binding_base_), ".*");
}

using ProxyEventTraceCallGetNewSamplesCallbackFixture = ProxyEventTracingFixture;
TEST_P(ProxyEventTraceCallGetNewSamplesCallbackFixture,
       TraceCallGetNewSamplesCallbackWillDispatchToBindingTracingRuntime)
{
    ITracingRuntime::TracePointDataId trace_point_data_id{10U};

    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceCallGetNewSamplesCallback
    TraceCallGetNewSamplesCallback(proxy_event_tracing_data_, proxy_event_binding_base_, trace_point_data_id);
}

TEST_P(ProxyEventTraceCallGetNewSamplesCallbackFixture,
       TraceSubscribeWillNotDispatchToBindingTracingRuntimeIfTracingDisabled)
{
    ITracingRuntime::TracePointDataId trace_point_data_id{10U};

    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceCallGetNewSamplesCallback
    TraceCallGetNewSamplesCallback(proxy_event_tracing_data_, proxy_event_binding_base_, trace_point_data_id);
}

TEST_P(ProxyEventTraceCallGetNewSamplesCallbackFixture,
       TraceCallGetNewSamplesCallbackWillDisableTracePointIfDisableInstanceErrorReturnedFromBindingTracingRuntime)
{
    ITracingRuntime::TracePointDataId trace_point_data_id{10U};

    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable trace point instance
    // error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceCallGetNewSamplesCallback
    TraceCallGetNewSamplesCallback(proxy_event_tracing_data_, proxy_event_binding_base_, trace_point_data_id);

    // Then the enable_new_samples_callback trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_new_samples_callback);
}

TEST_P(ProxyEventTraceCallGetNewSamplesCallbackFixture,
       TraceCallGetNewSamplesCallbackWillDisableTracePointIfDisableAllTracePointsErrorReturnedFromBindingTracingRuntime)
{
    ITracingRuntime::TracePointDataId trace_point_data_id{10U};

    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable all trace points error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceCallGetNewSamplesCallback
    TraceCallGetNewSamplesCallback(proxy_event_tracing_data_, proxy_event_binding_base_, trace_point_data_id);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

TEST_P(ProxyEventTraceCallGetNewSamplesCallbackFixture,
       TraceCallGetNewSamplesCallbackWillIgnoreUnknownErrorFromBindingTracingRuntime)
{
    ITracingRuntime::TracePointDataId trace_point_data_id{10U};

    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns an unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceCallGetNewSamplesCallback
    TraceCallGetNewSamplesCallback(proxy_event_tracing_data_, proxy_event_binding_base_, trace_point_data_id);

    // Then the enable_new_samples_callback trace point is still enabled
    EXPECT_TRUE(proxy_event_tracing_data_.enable_new_samples_callback);
}

using ProxyEventTraceCallGetNewSamplesCallbackDeathTest = ProxyEventTracingFixture;
TEST_F(ProxyEventTraceCallGetNewSamplesCallbackDeathTest,
       TraceCallGetNewSamplesCallbackWithInvalidTraceServiceElementTypeTerminates)
{
    ITracingRuntime::TracePointDataId trace_point_data_id{10U};

    // Given a ProxyEventTracingData with an invalid element type and all trace points enabled
    WithAProxyEventTracingDataWithInvalidElementType().WithAllTracePointsEnabled();

    // When calling TraceCallGetNewSamplesCallback
    // Then we terminate
    EXPECT_DEATH(
        TraceCallGetNewSamplesCallback(proxy_event_tracing_data_, proxy_event_binding_base_, trace_point_data_id),
        ".*");
}

using ProxyEventTraceCallReceiveHandlerFixture = ProxyEventTracingFixture;
TEST_P(ProxyEventTraceCallReceiveHandlerFixture, TraceCallReceiveHandlerWillDispatchToBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceCallReceiveHandler
    TraceCallReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceCallReceiveHandlerFixture, TraceSubscribeWillNotDispatchToBindingTracingRuntimeIfTracingDisabled)
{
    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceCallReceiveHandler
    TraceCallReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceCallReceiveHandlerFixture,
       TraceCallReceiveHandlerWillDisableTracePointIfDisableInstanceErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable trace point instance
    // error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceCallReceiveHandler
    TraceCallReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_call_receive_handler trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_call_receive_handler);
}

TEST_P(ProxyEventTraceCallReceiveHandlerFixture,
       TraceCallReceiveHandlerWillDisableTracePointIfDisableAllTracePointsErrorReturnedFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns a disable all trace points error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceCallReceiveHandler
    TraceCallReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

TEST_P(ProxyEventTraceCallReceiveHandlerFixture, TraceCallReceiveHandlerWillIgnoreUnknownErrorFromBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding which returns an unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceCallReceiveHandler
    TraceCallReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_call_receive_handler trace point is still enabled
    EXPECT_TRUE(proxy_event_tracing_data_.enable_call_receive_handler);
}

using ProxyEventTraceCallReceiveHandlerDeathTest = ProxyEventTracingFixture;
TEST_F(ProxyEventTraceCallReceiveHandlerDeathTest, TraceCallReceiveHandlerWithInvalidTraceServiceElementTypeTerminates)
{
    // Given a ProxyEventTracingData with an invalid element type and all trace points enabled
    WithAProxyEventTracingDataWithInvalidElementType().WithAllTracePointsEnabled();

    // When calling TraceCallReceiveHandler
    // Then we terminate
    EXPECT_DEATH(TraceCallReceiveHandler(proxy_event_tracing_data_, proxy_event_binding_base_), ".*");
}

TEST_P(ProxyEventCreateTracingGetNewSamplesCallbackFixture,
       GetNewSamplesCallbackWillDispatchToBindingTracingRuntimeWhenNewSamplesCallbackTracePointEnabled)
{
    // Given a ProxyEventTracingData with all trace points enabled and a TracingGetNewSamplesCallback
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    auto dummy_receiver = [](SamplePtr<TestSampleType>) {};
    auto get_new_samples_callback = CreateTracingGetNewSamplesCallback<TestSampleType, decltype(dummy_receiver)>(
        proxy_event_tracing_data_, proxy_event_binding_base_, std::move(dummy_receiver));

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling the created GetNewSamplesCallback
    get_new_samples_callback(nullptr, 1U);
}

TEST_P(ProxyEventCreateTracingGetNewSamplesCallbackFixture,
       GetNewSamplesCallbackWillNotDispatchToBindingTracingRuntimeWhenNewSamplesCallbackTracePointDisabled)
{
    // Given a ProxyEventTracingData with enable_new_samples_callback disabled and a TracingGetNewSamplesCallback
    WithAValidProxyEventTracingData();
    proxy_event_tracing_data_.enable_new_samples_callback = false;

    auto dummy_receiver = [](SamplePtr<TestSampleType>) {};
    auto get_new_samples_callback = CreateTracingGetNewSamplesCallback<TestSampleType, decltype(dummy_receiver)>(
        proxy_event_tracing_data_, proxy_event_binding_base_, std::move(dummy_receiver));

    // Expecting TraceData will never be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).Times(0);

    // When calling the created GetNewSamplesCallback
    get_new_samples_callback(nullptr, 1U);
}

using ProxyEventTraceSubscriptionStateChangedFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceSubscriptionStateChangedFixture,
                         ProxyEventTraceSubscriptionStateChangedFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(ProxyEventTraceSubscriptionStateChangedFixture, TraceSubscriptionStateChangedWillDispatchToBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceSubscriptionStateChanged with SUBSCRIBED state
    TraceSubscriptionStateChanged(proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kSubscribed);
}

TEST_P(ProxyEventTraceSubscriptionStateChangedFixture, TraceSubscriptionStateChangedWillNotDispatchIfTracingDisabled)
{
    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceSubscriptionStateChanged
    TraceSubscriptionStateChanged(
        proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kNotSubscribed);
}

TEST_P(ProxyEventTraceSubscriptionStateChangedFixture,
       TraceSubscriptionStateChangedWillDisableTracePointIfDisableInstanceErrorReturned)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return disable instance error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceSubscriptionStateChanged
    TraceSubscriptionStateChanged(proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kSubscribed);

    // Then the enable_subscription_state_changed trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_subscription_state_changed);
}

TEST_P(ProxyEventTraceSubscriptionStateChangedFixture,
       TraceSubscriptionStateChangedWillDisableAllTracePointsIfDisableAllErrorReturned)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return disable all error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceSubscriptionStateChanged
    TraceSubscriptionStateChanged(
        proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kNotSubscribed);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

TEST_P(ProxyEventTraceSubscriptionStateChangedFixture, TraceSubscriptionStateChangedWillIgnoreUnknownError)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceSubscriptionStateChanged
    TraceSubscriptionStateChanged(proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kSubscribed);

    // Then the trace point remains enabled
    EXPECT_TRUE(proxy_event_tracing_data_.enable_subscription_state_changed);
}

using ProxyEventTraceSetSubscriptionStateChangeHandlerFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceSetSubscriptionStateChangeHandlerFixture,
                         ProxyEventTraceSetSubscriptionStateChangeHandlerFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(ProxyEventTraceSetSubscriptionStateChangeHandlerFixture,
       TraceSetSubscriptionStateChangeHandlerWillDispatchToBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceSetSubscriptionStateChangeHandler
    TraceSetSubscriptionStateChangeHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceSetSubscriptionStateChangeHandlerFixture,
       TraceSetSubscriptionStateChangeHandlerWillNotDispatchIfTracingDisabled)
{
    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceSetSubscriptionStateChangeHandler
    TraceSetSubscriptionStateChangeHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceSetSubscriptionStateChangeHandlerFixture,
       TraceSetSubscriptionStateChangeHandlerWillDisableTracePointIfDisableInstanceErrorReturned)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return disable instance error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceSetSubscriptionStateChangeHandler
    TraceSetSubscriptionStateChangeHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_set_subscription_state_change_handler trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_set_subcription_state_change_handler);
}

TEST_P(ProxyEventTraceSetSubscriptionStateChangeHandlerFixture,
       TraceSetSubscriptionStateChangeHandlerWillDisableAllTracePointsIfDisableAllErrorReturned)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return disable all error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceSetSubscriptionStateChangeHandler
    TraceSetSubscriptionStateChangeHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

using ProxyEventTraceUnsetSubscriptionStateChangeHandlerFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceUnsetSubscriptionStateChangeHandlerFixture,
                         ProxyEventTraceUnsetSubscriptionStateChangeHandlerFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(ProxyEventTraceUnsetSubscriptionStateChangeHandlerFixture,
       TraceUnsetSubscriptionStateChangeHandlerWillDispatchToBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceUnsetSubscriptionStateChangeHandler
    TraceUnsetSubscriptionStateChangeHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceUnsetSubscriptionStateChangeHandlerFixture,
       TraceUnsetSubscriptionStateChangeHandlerWillNotDispatchIfTracingDisabled)
{
    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceUnsetSubscriptionStateChangeHandler
    TraceUnsetSubscriptionStateChangeHandler(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventTraceUnsetSubscriptionStateChangeHandlerFixture,
       TraceUnsetSubscriptionStateChangeHandlerWillDisableTracePointIfDisableInstanceErrorReturned)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return disable instance error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceUnsetSubscriptionStateChangeHandler
    TraceUnsetSubscriptionStateChangeHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then the enable_unset_subscription_state_change_handler trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_unset_subscription_state_change_handler);
}

TEST_P(ProxyEventTraceUnsetSubscriptionStateChangeHandlerFixture,
       TraceUnsetSubscriptionStateChangeHandlerWillDisableAllTracePointsIfDisableAllErrorReturned)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return disable all error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceUnsetSubscriptionStateChangeHandler
    TraceUnsetSubscriptionStateChangeHandler(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

using ProxyEventTraceCallSubscriptionStateChangeHandlerFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventTraceCallSubscriptionStateChangeHandlerFixture,
                         ProxyEventTraceCallSubscriptionStateChangeHandlerFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(ProxyEventTraceCallSubscriptionStateChangeHandlerFixture,
       TraceCallSubscriptionStateChangeHandlerWillDispatchToBindingTracingRuntime)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _));

    // When calling TraceCallSubscriptionStateChangeHandler with SUBSCRIBED state
    TraceCallSubscriptionStateChangeHandler(
        proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kSubscribed);
}

TEST_P(ProxyEventTraceCallSubscriptionStateChangeHandlerFixture,
       TraceCallSubscriptionStateChangeHandlerWillNotDispatchIfTracingDisabled)
{
    // Given a ProxyEventTracingData with all trace points disabled
    WithAValidProxyEventTracingData();

    // Expecting TraceData will not be called on the TracingRuntime binding
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).Times(0);

    // When calling TraceCallSubscriptionStateChangeHandler
    TraceCallSubscriptionStateChangeHandler(
        proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kNotSubscribed);
}

TEST_P(ProxyEventTraceCallSubscriptionStateChangeHandlerFixture,
       TraceCallSubscriptionStateChangeHandlerWillDisableTracePointIfDisableInstanceErrorReturned)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return disable instance error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance)));

    // When calling TraceCallSubscriptionStateChangeHandler
    TraceCallSubscriptionStateChangeHandler(
        proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kSubscribed);

    // Then the enable_call_subscription_state_change_handler trace point is disabled
    EXPECT_FALSE(proxy_event_tracing_data_.enable_call_subscription_state_change_handler);
}

TEST_P(ProxyEventTraceCallSubscriptionStateChangeHandlerFixture,
       TraceCallSubscriptionStateChangeHandlerWillDisableAllTracePointsIfDisableAllErrorReturned)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return disable all error
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _))
        .WillByDefault(Return(MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints)));

    // When calling TraceCallSubscriptionStateChangeHandler
    TraceCallSubscriptionStateChangeHandler(
        proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kNotSubscribed);

    // Then all trace points are disabled
    EXPECT_TRUE(AreAllTracePointsDisabled(proxy_event_tracing_data_));
}

TEST_P(ProxyEventTraceCallSubscriptionStateChangeHandlerFixture,
       TraceCallSubscriptionStateChangeHandlerWillIgnoreUnknownError)
{
    // Given a ProxyEventTracingData with all trace points enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called and return unknown error
    const auto unknown_error_code = static_cast<TraceErrorCode>(100U);
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(unknown_error_code)));

    // When calling TraceCallSubscriptionStateChangeHandler
    TraceCallSubscriptionStateChangeHandler(
        proxy_event_tracing_data_, proxy_event_binding_base_, SubscriptionState::kSubscribed);

    // Then the trace point remains enabled
    EXPECT_TRUE(proxy_event_tracing_data_.enable_call_subscription_state_change_handler);
}

using ProxyEventSetupSubscriptionStateChangeTracingFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventSetupSubscriptionStateChangeTracingFixture,
                         ProxyEventSetupSubscriptionStateChangeTracingFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(ProxyEventSetupSubscriptionStateChangeTracingFixture,
       SetupSubscriptionStateChangeTracingWillInstallCallbackWhenEnabled)
{
    // Given a ProxyEventTracingData with subscription state change tracing enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting SetSubscriptionStateChangeTracingCallback will be called on binding
    EXPECT_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeTracingCallback(_)).Times(1);

    // When calling SetupSubscriptionStateChangeTracing
    SetupSubscriptionStateChangeTracing(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventSetupSubscriptionStateChangeTracingFixture,
       SetupSubscriptionStateChangeTracingWillNotInstallCallbackWhenDisabled)
{
    // Given a ProxyEventTracingData with subscription state change tracing disabled
    WithAValidProxyEventTracingData();
    proxy_event_tracing_data_.enable_subscription_state_changed = false;

    // Expecting SetSubscriptionStateChangeTracingCallback will not be called
    EXPECT_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeTracingCallback(_)).Times(0);

    // When calling SetupSubscriptionStateChangeTracing
    SetupSubscriptionStateChangeTracing(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventSetupSubscriptionStateChangeTracingFixture,
       SetupSubscriptionStateChangeTracingCallbackWillInvokeTraceFunction)
{
    // Given a ProxyEventTracingData with subscription state change tracing enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting SetSubscriptionStateChangeTracingCallback will be called
    EXPECT_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeTracingCallback(_)).Times(1);

    // And expecting Trace will be called when the subscription state machine invokes the callback
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).Times(1);

    // Capture and store the callback to invoke it
    std::optional<score::cpp::callback<void(SubscriptionState), 64U>> captured_callback;
    ON_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeTracingCallback(_))
        .WillByDefault([&captured_callback](score::cpp::callback<void(SubscriptionState), 64U> callback) {
            captured_callback = std::move(callback);
        });

    // When calling SetupSubscriptionStateChangeTracing
    SetupSubscriptionStateChangeTracing(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then verify the callback was captured and can be invoked
    ASSERT_TRUE(captured_callback.has_value());

    // When the subscription state machine invokes the callback
    captured_callback.value()(SubscriptionState::kSubscribed);

    // Then Trace should have been called
}

TEST_P(ProxyEventSetupSubscriptionStateChangeTracingFixture,
       SetupSubscriptionStateChangeTracingCallbackWillHandleMultipleStates)
{
    // Given a ProxyEventTracingData with subscription state change tracing enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called for each state change
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).Times(3);

    // Capture and store the callback
    std::optional<score::cpp::callback<void(SubscriptionState), 64U>> captured_callback;
    ON_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeTracingCallback(_))
        .WillByDefault([&captured_callback](score::cpp::callback<void(SubscriptionState), 64U> callback) {
            captured_callback = std::move(callback);
        });

    // When calling SetupSubscriptionStateChangeTracing
    SetupSubscriptionStateChangeTracing(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then verify the callback was captured
    ASSERT_TRUE(captured_callback.has_value());

    // When invoking the callback with different subscription states
    captured_callback.value()(SubscriptionState::kNotSubscribed);
    captured_callback.value()(SubscriptionState::kSubscriptionPending);
    captured_callback.value()(SubscriptionState::kSubscribed);

    // Then Trace should have been called 3 times
}

using ProxyEventSetupSubscriptionStateChangeHandlerTracingFixture = ProxyEventTracingFixture;
INSTANTIATE_TEST_SUITE_P(ProxyEventSetupSubscriptionStateChangeHandlerTracingFixture,
                         ProxyEventSetupSubscriptionStateChangeHandlerTracingFixture,
                         ::testing::Values(ServiceElementType::EVENT, ServiceElementType::FIELD));

TEST_P(ProxyEventSetupSubscriptionStateChangeHandlerTracingFixture,
       SetupSubscriptionStateChangeHandlerTracingWillInstallCallbackWhenEnabled)
{
    // Given a ProxyEventTracingData with call subscription state change handler tracing enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting SetSubscriptionStateChangeHandlerTracingCallback will be called on binding
    EXPECT_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeHandlerTracingCallback(_)).Times(1);

    // When calling SetupSubscriptionStateChangeHandlerTracing
    SetupSubscriptionStateChangeHandlerTracing(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventSetupSubscriptionStateChangeHandlerTracingFixture,
       SetupSubscriptionStateChangeHandlerTracingWillNotInstallCallbackWhenDisabled)
{
    // Given a ProxyEventTracingData with call subscription state change handler tracing disabled
    WithAValidProxyEventTracingData();
    proxy_event_tracing_data_.enable_call_subscription_state_change_handler = false;

    // Expecting SetSubscriptionStateChangeHandlerTracingCallback will not be called
    EXPECT_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeHandlerTracingCallback(_)).Times(0);

    // When calling SetupSubscriptionStateChangeHandlerTracing
    SetupSubscriptionStateChangeHandlerTracing(proxy_event_tracing_data_, proxy_event_binding_base_);
}

TEST_P(ProxyEventSetupSubscriptionStateChangeHandlerTracingFixture,
       SetupSubscriptionStateChangeHandlerTracingCallbackWillInvokeTraceFunction)
{
    // Given a ProxyEventTracingData with call subscription state change handler tracing enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting SetSubscriptionStateChangeHandlerTracingCallback will be called
    EXPECT_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeHandlerTracingCallback(_)).Times(1);

    // And expecting Trace will be called when the subscription state machine invokes the callback
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).Times(1);

    // Capture and store the callback to invoke it
    std::optional<score::cpp::callback<void(SubscriptionState), 64U>> captured_callback;
    ON_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeHandlerTracingCallback(_))
        .WillByDefault([&captured_callback](score::cpp::callback<void(SubscriptionState), 64U> callback) {
            captured_callback = std::move(callback);
        });

    // When calling SetupSubscriptionStateChangeHandlerTracing
    SetupSubscriptionStateChangeHandlerTracing(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then verify the callback was captured and can be invoked
    ASSERT_TRUE(captured_callback.has_value());

    // When the subscription state machine invokes the callback before user handler
    captured_callback.value()(SubscriptionState::kSubscribed);

    // Then Trace should have been called
}

TEST_P(ProxyEventSetupSubscriptionStateChangeHandlerTracingFixture,
       SetupSubscriptionStateChangeHandlerTracingCallbackWillHandleMultipleStates)
{
    // Given a ProxyEventTracingData with call subscription state change handler tracing enabled
    WithAValidProxyEventTracingData().WithAllTracePointsEnabled();

    // Expecting Trace will be called for each handler invocation
    EXPECT_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).Times(3);

    // Capture and store the callback
    std::optional<score::cpp::callback<void(SubscriptionState), 64U>> captured_callback;
    ON_CALL(proxy_event_binding_base_, SetSubscriptionStateChangeHandlerTracingCallback(_))
        .WillByDefault([&captured_callback](score::cpp::callback<void(SubscriptionState), 64U> callback) {
            captured_callback = std::move(callback);
        });

    // When calling SetupSubscriptionStateChangeHandlerTracing
    SetupSubscriptionStateChangeHandlerTracing(proxy_event_tracing_data_, proxy_event_binding_base_);

    // Then verify the callback was captured
    ASSERT_TRUE(captured_callback.has_value());

    // When invoking the callback with different subscription states
    captured_callback.value()(SubscriptionState::kNotSubscribed);
    captured_callback.value()(SubscriptionState::kSubscriptionPending);
    captured_callback.value()(SubscriptionState::kSubscribed);

    // Then Trace should have been called 3 times
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
