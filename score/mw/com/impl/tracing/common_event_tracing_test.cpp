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
#include "score/mw/com/impl/tracing/common_event_tracing.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/tracing/test/runtime_mock_guard.h"
#include "score/mw/com/impl/tracing/tracing_runtime_mock.h"
#include "score/mw/com/impl/tracing/type_erased_sample_ptr.h"

#include "score/result/result.h"

#include <score/optional.hpp>
#include <score/utility.hpp>

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl::tracing
{
namespace
{

using namespace ::testing;

constexpr std::string_view kServiceElementName{"ServiceElement1"};
const auto kInstanceSpecifier = InstanceSpecifier::Create("/bla/blub/specifier").value();
constexpr auto kServiceElementType = ServiceElementType::EVENT;
const ConfigurationStore kConfigStore{
    kInstanceSpecifier,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    LolaServiceId{1U},
    LolaServiceInstanceId{2U},
};

class CommonEventTracingFixture : public ::testing::Test
{
  public:
    CommonEventTracingFixture()
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(&tracing_runtime_mock_));
    }

    ServiceElementInstanceIdentifierView service_element_instance_identifier_view_{
        GetServiceElementInstanceIdentifierView(kConfigStore.GetInstanceIdentifier(),
                                                kServiceElementName,
                                                kServiceElementType)};
    TracingRuntime::TracePointType trace_point_{ProxyEventTracePointType::SET_RECEIVE_HANDLER};
    BindingType binding_type_{BindingType::kLoLa};
    const std::pair<const void*, std::size_t> local_data_chunk_{
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(10U)),
        100U};
    TracingRuntime::TracePointDataId trace_point_data_id_{10U};
    ServiceElementTracingData service_element_tracing_data_{0U, 1U};

    TypeErasedSamplePtr type_erased_sample_ptr_{std::make_unique<int>(10U)};
    const std::pair<const void*, std::size_t> shm_data_chunk_{
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(20U)),
        200U};

    TracingRuntimeMock tracing_runtime_mock_{};
    RuntimeMockGuard runtime_mock_guard_{};
};

TEST(CommonEventTracingTest, CallingGetServiceElementInstanceIdentifierViewWillReturnStructFilledWithProvidedData)
{
    // When calling GetServiceElementInstanceIdentifierView
    const auto service_element_instance_identifier_view = GetServiceElementInstanceIdentifierView(
        kConfigStore.GetInstanceIdentifier(), kServiceElementName, kServiceElementType);

    // Then the resulting struct should be filled with the data provided to GetServiceElementInstanceIdentifierView
    EXPECT_EQ(service_element_instance_identifier_view.service_element_identifier_view.service_type_name,
              kConfigStore.service_identifier_.ToString());
    EXPECT_EQ(service_element_instance_identifier_view.service_element_identifier_view.service_element_name,
              kServiceElementName);
    EXPECT_EQ(service_element_instance_identifier_view.service_element_identifier_view.service_element_type,
              kServiceElementType);
    EXPECT_EQ(service_element_instance_identifier_view.instance_specifier, kInstanceSpecifier.ToString());
}

using CommonEventTracingLocalTraceDataFixture = CommonEventTracingFixture;
TEST_F(CommonEventTracingLocalTraceDataFixture, CallingTraceDataWillReturnSuccessIfBindingReturnsSuccess)
{
    // Expecting that TraceData will be called on the tracing runtime binding which returns a valid result
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(ResultBlank{}));

    // When calling TraceData with local data chunk
    const auto trace_data_result = TraceData(service_element_instance_identifier_view_,
                                             trace_point_,
                                             binding_type_,
                                             local_data_chunk_,
                                             trace_point_data_id_);

    // Then a valid result is returned
    EXPECT_TRUE(trace_data_result.has_value());
}

TEST_F(CommonEventTracingLocalTraceDataFixture, CallingTraceDataWillDispatchToBindingWithProvidedArgs)
{
    // Expecting that TraceData will be called on the tracing runtime binding with the arguments provided to TraceData
    EXPECT_CALL(tracing_runtime_mock_,
                Trace(binding_type_,
                      service_element_instance_identifier_view_,
                      trace_point_,
                      score::cpp::optional<TracingRuntime::TracePointDataId>{trace_point_data_id_},
                      local_data_chunk_.first,
                      local_data_chunk_.second));

    // When calling TraceData with local data chunk
    score::cpp::ignore = TraceData(service_element_instance_identifier_view_,
                            trace_point_,
                            binding_type_,
                            local_data_chunk_,
                            trace_point_data_id_);
}

TEST_F(CommonEventTracingLocalTraceDataFixture, CallingTraceDataWillErrorIfBindingReturnsError)
{
    // Expecting that TraceData will be called on the tracing runtime binding which returns an error
    const auto expected_error = ComErrc::kBindingFailure;
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(expected_error)));

    // When calling TraceData with local data chunk
    const auto trace_data_result = TraceData(service_element_instance_identifier_view_,
                                             trace_point_,
                                             binding_type_,
                                             local_data_chunk_,
                                             trace_point_data_id_);

    // Then an error is returned
    ASSERT_FALSE(trace_data_result.has_value());
    EXPECT_EQ(trace_data_result.error(), expected_error);
}

using CommonEventTracingLocalTraceDataDeathTest = CommonEventTracingLocalTraceDataFixture;
TEST_F(CommonEventTracingLocalTraceDataDeathTest, CallingTraceDataWhenTracingRuntimeIsNullptrTerminates)
{
    // Expecting that when getting the TracingRuntime from the Runtime a nullptr is returned
    ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(nullptr));

    // When calling TraceData with local data chunk
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = TraceData(service_element_instance_identifier_view_,
                                         trace_point_,
                                         binding_type_,
                                         local_data_chunk_,
                                         trace_point_data_id_),
                 ".*");
}

using CommonEventTracingShmTraceDataFixture = CommonEventTracingFixture;
TEST_F(CommonEventTracingShmTraceDataFixture, CallingTraceDataWillReturnSuccess)
{
    // Expecting that TraceShmData will be called on the tracing runtime binding which returns a valid result
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).WillByDefault(Return(ResultBlank{}));

    // When calling TraceShmData with a shm data chunk
    const auto trace_shm_data_result = TraceShmData(binding_type_,
                                                    service_element_tracing_data_,
                                                    service_element_instance_identifier_view_,
                                                    trace_point_,
                                                    trace_point_data_id_,
                                                    std::move(type_erased_sample_ptr_),
                                                    shm_data_chunk_);

    // Then a valid result is returned
    EXPECT_TRUE(trace_shm_data_result.has_value());
}

TEST_F(CommonEventTracingShmTraceDataFixture, CallingTraceDataWillDispatchToBindingWithProvidedArgs)
{
    // Expecting that TraceShmData will be called on the tracing runtime binding with the arguments provided to
    // TraceData
    EXPECT_CALL(tracing_runtime_mock_,
                Trace(binding_type_,
                      service_element_tracing_data_,
                      service_element_instance_identifier_view_,
                      trace_point_,
                      trace_point_data_id_,
                      _,
                      shm_data_chunk_.first,
                      shm_data_chunk_.second));

    // When calling TraceShmData with local data chunk
    score::cpp::ignore = TraceShmData(binding_type_,
                               service_element_tracing_data_,
                               service_element_instance_identifier_view_,
                               trace_point_,
                               trace_point_data_id_,
                               std::move(type_erased_sample_ptr_),
                               shm_data_chunk_);
}

TEST_F(CommonEventTracingShmTraceDataFixture, CallingTraceDataWillReturnErrorIfBindingReturnsError)
{
    // Expecting that TraceShmData will be called on the tracing runtime binding which returns an error
    const auto expected_error = ComErrc::kBindingFailure;
    ON_CALL(tracing_runtime_mock_, Trace(_, _, _, _, _, _, _, _)).WillByDefault(Return(MakeUnexpected(expected_error)));

    // When calling TraceShmData with a shm data chunk
    const auto trace_shm_data_result = TraceShmData(binding_type_,
                                                    service_element_tracing_data_,
                                                    service_element_instance_identifier_view_,
                                                    trace_point_,
                                                    trace_point_data_id_,
                                                    std::move(type_erased_sample_ptr_),
                                                    shm_data_chunk_);

    // Then an error is returned
    ASSERT_FALSE(trace_shm_data_result.has_value());
    EXPECT_EQ(trace_shm_data_result.error(), expected_error);
}

using CommonEventTracingShmTraceDataDeathTest = CommonEventTracingShmTraceDataFixture;
TEST_F(CommonEventTracingShmTraceDataDeathTest, CallingTraceDataWhenTracingRuntimeIsNullptrTerminates)
{
    // Expecting that when getting the TracingRuntime from the Runtime a nullptr is returned
    ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingRuntime()).WillByDefault(Return(nullptr));

    // When calling TraceData with a shm data chunk
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = TraceShmData(binding_type_,
                                            service_element_tracing_data_,
                                            service_element_instance_identifier_view_,
                                            trace_point_,
                                            trace_point_data_id_,
                                            std::move(type_erased_sample_ptr_),
                                            shm_data_chunk_),
                 ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
