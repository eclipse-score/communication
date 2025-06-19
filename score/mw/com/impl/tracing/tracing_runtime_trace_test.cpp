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
#include "score/mw/com/impl/tracing/configuration/proxy_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/proxy_field_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_field_trace_point_type.h"
#include "score/mw/com/impl/tracing/tracing_runtime.h"

#include "score/analysis/tracing/library/interface/trace_point_type.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/mw/com/impl/bindings/mock_binding/tracing/tracing_runtime.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/trace_error.h"
#include "score/mw/com/impl/tracing/tracing_test_resources.h"

#include "score/analysis/tracing/library/generic_trace_api/error_code/error_code.h"
#include "score/analysis/tracing/library/generic_trace_api/mocks/trace_library_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <utility>

namespace score::mw::com::impl::tracing
{

namespace
{
constexpr std::string_view kDummyServiceTypeName{"my_service_type"};
constexpr std::string_view kDummyElementName{"my_event"};
constexpr std::string_view kInstanceSpecifier{"/my_service_type_port"};

const void* const kLocalDataPtr{reinterpret_cast<void*>(static_cast<intptr_t>(500))};
constexpr std::size_t kLocalDataSize{8};
const score::cpp::optional<TracingRuntime::TracePointDataId> kEmptyDataId{};

const analysis::tracing::ServiceInstanceElement::EventIdType kServiceInstanceElementEventId = 42U;
const analysis::tracing::ServiceInstanceElement::VariantType kServiceInstanceElementVariant{
    kServiceInstanceElementEventId};
const analysis::tracing::ServiceInstanceElement kServiceInstanceElement{25, 1, 0, 1, kServiceInstanceElementVariant};

using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::WithArg;

class MySamplePtrType
{
  public:
    MySamplePtrType() = default;
    MySamplePtrType(MySamplePtrType& other) = delete;
    MySamplePtrType(MySamplePtrType&& other) = default;
};

class TracingRuntimeTraceFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        generic_trace_api_mock_ = std::make_unique<score::analysis::tracing::TraceLibraryMock>();
        std::unordered_map<BindingType, ITracingRuntimeBinding*> tracing_runtime_binding_map;
        tracing_runtime_binding_map.emplace(BindingType::kLoLa, &tracing_runtime_binding_mock_);
        EXPECT_CALL(tracing_runtime_binding_mock_, RegisterWithGenericTraceApi()).WillOnce(Return(true));

        unit_under_test_ = std::make_unique<TracingRuntime>(std::move(tracing_runtime_binding_map));
        ASSERT_TRUE(unit_under_test_);
        TracingRuntimeAttorney attorney{*unit_under_test_};
        EXPECT_EQ(attorney.GetFailureCounter(), 0U);
        EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    };

    void SetupTracingRuntimeBindingMockForShmDataTraceCall()
    {
        // Expect that a free slot for a sample pointer can be found with the index trace_context_id_
        ON_CALL(tracing_runtime_binding_mock_, EmplaceTypeErasedSamplePtr(_, service_element_tracing_data_))
            .WillByDefault(Return(trace_context_id_));
        // expect that UuT gets a valid ShmObject from the binding tracing runtime for the given service element
        // instance
        ON_CALL(tracing_runtime_binding_mock_, GetShmObjectHandle(dummy_service_element_instance_identifier_view_))
            .WillByDefault(Return(dummy_shm_object_handle_));
        // expect that UuT gets a valid ShmRegionStartAddress from the binding tracing runtime for the given service
        // element instance
        ON_CALL(tracing_runtime_binding_mock_,
                GetShmRegionStartAddress(dummy_service_element_instance_identifier_view_))
            .WillByDefault(Return(dummy_shm_object_start_address_));
        ON_CALL(tracing_runtime_binding_mock_, GetDataLossFlag()).WillByDefault(Return(false));
        ON_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillByDefault(Return(trace_client_id_));

        ON_CALL(tracing_runtime_binding_mock_,
                ConvertToTracingServiceInstanceElement(dummy_service_element_instance_identifier_view_))
            .WillByDefault(Return(kServiceInstanceElement));
    }

    void SetupTracingRuntimeBindingMockForLocalDataTraceCall()
    {
        ON_CALL(tracing_runtime_binding_mock_, GetDataLossFlag()).WillByDefault(Return(false));
        ON_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillByDefault(Return(trace_client_id_));

        ON_CALL(tracing_runtime_binding_mock_,
                ConvertToTracingServiceInstanceElement(dummy_service_element_instance_identifier_view_))
            .WillByDefault(Return(kServiceInstanceElement));
    }

    TypeErasedSamplePtr CreateDummySamplePtr()
    {
        return TypeErasedSamplePtr{MySamplePtrType()};
    }

    std::unique_ptr<TracingRuntime> unit_under_test_{nullptr};
    mock_binding::TracingRuntime tracing_runtime_binding_mock_{};
    std::unique_ptr<score::analysis::tracing::TraceLibraryMock> generic_trace_api_mock_{};
    ServiceElementIdentifierView dummy_service_element_identifier_view_{kDummyServiceTypeName,
                                                                        kDummyElementName,
                                                                        ServiceElementType::EVENT};
    ServiceElementInstanceIdentifierView dummy_service_element_instance_identifier_view_{
        dummy_service_element_identifier_view_,
        kInstanceSpecifier};
    TracingRuntime::TracePointDataId dummy_data_id_{42};
    void* dummy_shm_data_ptr_{reinterpret_cast<void*>(static_cast<intptr_t>(1111))};
    void* dummy_shm_object_start_address_ = reinterpret_cast<void*>(static_cast<uintptr_t>(1000));
    std::size_t dummy_shm_data_size_{23};
    analysis::tracing::ShmObjectHandle dummy_shm_object_handle_{77};
    ServiceElementTracingData service_element_tracing_data_{0, 2};
    impl::tracing::ITracingRuntimeBinding::TraceContextId trace_context_id_{1U};
    analysis::tracing::TraceClientId trace_client_id_{1};
};

class TracingRuntimeTraceParamaterisedFixture : public TracingRuntimeTraceFixture,
                                                public ::testing::WithParamInterface<TracingRuntime::TracePointType>
{
  public:
    TracingRuntime::TracePointType trace_point_type_{GetParam()};
};

using TracingRuntimeTraceShmParamaterisedFixture = TracingRuntimeTraceParamaterisedFixture;
INSTANTIATE_TEST_CASE_P(TracingRuntimeTraceShmParamaterisedFixture,
                        TracingRuntimeTraceShmParamaterisedFixture,
                        ::testing::Values(SkeletonEventTracePointType::SEND,
                                          SkeletonEventTracePointType::SEND_WITH_ALLOCATE,
                                          SkeletonFieldTracePointType::UPDATE,
                                          SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE));

using TracingRuntimeTraceLocalParamaterisedFixture = TracingRuntimeTraceParamaterisedFixture;
INSTANTIATE_TEST_CASE_P(TracingRuntimeTraceLocalParamaterisedFixture,
                        TracingRuntimeTraceLocalParamaterisedFixture,
                        ::testing::Values(ProxyEventTracePointType::GET_NEW_SAMPLES,
                                          ProxyEventTracePointType::SUBSCRIBE,
                                          ProxyFieldTracePointType::GET_NEW_SAMPLES,
                                          ProxyFieldTracePointType::SUBSCRIBE));

using TracingRuntimeTraceInvalidTracePointTypeParamaterisedDeathTest = TracingRuntimeTraceParamaterisedFixture;
INSTANTIATE_TEST_CASE_P(TracingRuntimeTraceInvalidTracePointTypeParamaterisedDeathTest,
                        TracingRuntimeTraceInvalidTracePointTypeParamaterisedDeathTest,
                        ::testing::Values(ProxyEventTracePointType::INVALID,
                                          SkeletonEventTracePointType::INVALID,
                                          ProxyFieldTracePointType::INVALID,
                                          SkeletonFieldTracePointType::INVALID));

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, CanConstructTracingRuntime)
{
    // When creating a tracingRuntime
    // Then a valid TracingRuntime is created
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, CallingTraceDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-18200105, SCR-18222321");
    RecordProperty(
        "Description",
        "Checks whether the right Trace call is done for data residing in shared-mem (SCR-18200105) and the right "
        "usage of ShmDataChunkList (SCR-18222321).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    analysis::tracing::SharedMemoryLocation root_chunk_memory_location{
        dummy_shm_object_handle_,
        static_cast<size_t>(
            memory::shared::SubtractPointersBytes(dummy_shm_data_ptr_, dummy_shm_object_start_address_))};
    analysis::tracing::SharedMemoryChunk root_chunk{root_chunk_memory_location, dummy_shm_data_size_};
    analysis::tracing::ShmDataChunkList expected_shm_chunk_list{root_chunk};

    EXPECT_CALL(*generic_trace_api_mock_.get(),
                Trace(trace_client_id_, _, Eq(ByRef(expected_shm_chunk_list)), trace_context_id_))
        .WillOnce(Return(analysis::tracing::TraceResult{}));

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);
    EXPECT_TRUE(result.has_value());
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, CallingTraceWillClearDataLossFlagOnSuccess)
{
    RecordProperty("Verifies", "SCR-18398053");
    RecordProperty("Description", "Checks reset of the data loss flag after successful Trace call.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    EXPECT_CALL(tracing_runtime_binding_mock_, SetDataLossFlag(false)).Times(1);

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    EXPECT_TRUE(result.has_value());
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, CallingTraceWillIndicateThatShmIsCurrentlyBeingTraced)
{
    RecordProperty("Verifies", "SCR-18390315");
    RecordProperty("Description",
                   "Calling Trace will notify the binding that data in shared memory is currently being traced.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    // then EmplaceTypeErasedSamplePtr will be called on the binding, which indicates to the binding that tracing of
    // shared memory data is currently active.
    EXPECT_CALL(tracing_runtime_binding_mock_, EmplaceTypeErasedSamplePtr(_, service_element_tracing_data_))
        .WillOnce(Return(trace_context_id_));

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    EXPECT_TRUE(result.has_value());
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture,
       CallingTraceWhileShmIsCurrentlyBeingTracedWillNotTraceAndWillSetDataLossFlag)
{
    RecordProperty("Verifies", "SCR-18391193, SCR-18398043");
    RecordProperty("Description",
                   "Calling Trace when the binding indicates that shared memory is currently being traced will not "
                   "Trace and will set the data loss flag (SCR-18391193). The data loss flag is stored in the binding "
                   "TracingRuntime (SCR-18398043)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    // expect, that there are no available tracing slots for the service element
    EXPECT_CALL(tracing_runtime_binding_mock_, EmplaceTypeErasedSamplePtr(_, service_element_tracing_data_))
        .WillOnce(Return(std::optional<impl::tracing::ITracingRuntimeBinding::TraceContextId>{}));
    // and that the data loss flag will be set
    EXPECT_CALL(tracing_runtime_binding_mock_, SetDataLossFlag(true));

    // and Trace will not be called on the binding
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _, trace_context_id_)).Times(0);

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    EXPECT_TRUE(result.has_value());
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, TraceShmDataOK_RetryShmObjectRegistration)
{
    RecordProperty("Verifies", "SCR-18200105, SCR-18222321, SCR-18398047, SCR-18172392");
    RecordProperty(
        "Description",
        "Checks whether the right Trace call is done for data residing in shared-mem (SCR-18200105) and the right "
        "usage of ShmDataChunkList (SCR-18222321). Also checks the transmission data loss flag (SCR-18398047). "
        "Additionally it tests, that re-registration of a previous/once failed ShmObject registration is done "
        "(SCR-18172392)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    analysis::tracing::ShmObjectHandle shm_object_handle{77};
    memory::shared::ISharedMemoryResource::FileDescriptor shm_file_descriptor{1};

    // expect, that a slot for a sample pointer can be found at the index trace_context_id
    EXPECT_CALL(tracing_runtime_binding_mock_, EmplaceTypeErasedSamplePtr(_, service_element_tracing_data_))
        .WillOnce(Return(trace_context_id_));
    // expect, that the binding specific tracing runtime doesn't have a ShmObjectHandle for the given identifier
    EXPECT_CALL(tracing_runtime_binding_mock_, GetShmObjectHandle(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(score::cpp::optional<analysis::tracing::ShmObjectHandle>{}));
    // then expect, that UuT calls GetCachedFileDescriptorForReregisteringShmObject() on binding specific tracing
    // runtime
    EXPECT_CALL(tracing_runtime_binding_mock_,
                GetCachedFileDescriptorForReregisteringShmObject(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(score::cpp::optional<std::pair<memory::shared::ISharedMemoryResource::FileDescriptor, void*>>{
            {shm_file_descriptor, dummy_shm_object_start_address_}}));
    // and expect, that it then retries the RegisterShmObject() call on the GenericTraceAPI, which is successful and
    // returns a ShmObjectHandle
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterShmObject(trace_client_id_, shm_file_descriptor))
        .WillOnce(Return(analysis::tracing::RegisterSharedMemoryObjectResult{shm_object_handle}));
    EXPECT_CALL(
        tracing_runtime_binding_mock_,
        RegisterShmObject(
            dummy_service_element_instance_identifier_view_, shm_object_handle, dummy_shm_object_start_address_))
        .Times(1);
    EXPECT_CALL(tracing_runtime_binding_mock_,
                GetShmRegionStartAddress(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(dummy_shm_object_start_address_));
    EXPECT_CALL(tracing_runtime_binding_mock_, GetDataLossFlag()).WillOnce(Return(false));
    EXPECT_CALL(tracing_runtime_binding_mock_, SetDataLossFlag(false)).Times(1);
    EXPECT_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillRepeatedly(Return(trace_client_id_));

    analysis::tracing::ServiceInstanceElement::VariantType variant{};
    analysis::tracing::ServiceInstanceElement service_instance_element{25, 1, 0, 1, variant};
    EXPECT_CALL(tracing_runtime_binding_mock_,
                ConvertToTracingServiceInstanceElement(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(service_instance_element));

    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _, trace_context_id_))
        .WillOnce(Return(analysis::tracing::TraceResult{}));

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    EXPECT_TRUE(result.has_value());
    TracingRuntimeAttorney attorney{*unit_under_test_};
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    EXPECT_EQ(attorney.GetFailureCounter(), 0);
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, TraceShmDataNOK_RetryShmObjectRegistrationFailsWithNonFatalError)
{
    RecordProperty("Verifies", "SCR-18200105, SCR-18222321, SCR-18398047, SCR-18172392");
    RecordProperty(
        "Description",
        "Checks whether the right Trace call is done for data residing in shared-mem (SCR-18200105) and the right "
        "usage of ShmDataChunkList (SCR-18222321). Also checks the transmission data loss flag (SCR-18398047). "
        "Additionally it tests, that re-registration of a previous/once failed ShmObject registration is tried."
        "(SCR-18172392)");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    memory::shared::ISharedMemoryResource::FileDescriptor shm_file_descriptor{1};

    // expect, that the binding specific tracing runtime doesn't have a ShmObjectHandle for the given identifier
    EXPECT_CALL(tracing_runtime_binding_mock_, GetShmObjectHandle(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(score::cpp::optional<analysis::tracing::ShmObjectHandle>{}));
    // then expect, that UuT calls GetCachedFileDescriptorForReregisteringShmObject() on binding specific tracing
    // runtime
    EXPECT_CALL(tracing_runtime_binding_mock_,
                GetCachedFileDescriptorForReregisteringShmObject(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(score::cpp::optional<std::pair<memory::shared::ISharedMemoryResource::FileDescriptor, void*>>{
            {shm_file_descriptor, dummy_shm_object_start_address_}}));
    // expect, that UuT calls GetTraceClientId() on the binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillOnce(Return(trace_client_id_));
    // and expect, that it then retries the RegisterShmObject() call on the GenericTraceAPI, which fails with some error
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterShmObject(trace_client_id_, shm_file_descriptor))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kNotEnoughMemoryRecoverable)));
    // and expect, that UuT clears the cached file descriptor to avoid further retries for the failed shm-object.
    EXPECT_CALL(tracing_runtime_binding_mock_,
                ClearCachedFileDescriptorForReregisteringShmObject(dummy_service_element_instance_identifier_view_))
        .Times(1);

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TraceErrorCode::TraceErrorDisableTracePointInstance);
    TracingRuntimeAttorney attorney{*unit_under_test_};
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    EXPECT_EQ(attorney.GetFailureCounter(), 0);
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, TraceShmDataNOK_RetryShmObjectFailsWithFatalErrorDisabledTracing)
{
    RecordProperty("Verifies", "SCR-18200105, SCR-18222321, SCR-18398047, SCR-18172392");
    RecordProperty(
        "Description",
        "Checks whether the right Trace call is done for data residing in shared-mem (SCR-18200105) and the right "
        "usage of ShmDataChunkList (SCR-18222321). Also checks the transmission data loss flag (SCR-18398047). "
        "Additionally it tests, that re-registration of a previous/once failed ShmObject registration is tried."
        "(SCR-18172392)");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    memory::shared::ISharedMemoryResource::FileDescriptor shm_file_descriptor{1};

    // expect, that the binding specific tracing runtime doesn't have a ShmObjectHandle for the given identifier
    EXPECT_CALL(tracing_runtime_binding_mock_, GetShmObjectHandle(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(score::cpp::optional<analysis::tracing::ShmObjectHandle>{}));
    // then expect, that UuT calls GetCachedFileDescriptorForReregisteringShmObject() on binding specific tracing
    // runtime
    EXPECT_CALL(tracing_runtime_binding_mock_,
                GetCachedFileDescriptorForReregisteringShmObject(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(score::cpp::optional<std::pair<memory::shared::ISharedMemoryResource::FileDescriptor, void*>>{
            {shm_file_descriptor, dummy_shm_object_start_address_}}));
    // expect, that UuT calls GetTraceClientId() on the binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillOnce(Return(trace_client_id_));
    // and expect, that it then retries the RegisterShmObject() call on the GenericTraceAPI, which fails with a terminal
    // fatal error
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterShmObject(trace_client_id_, shm_file_descriptor))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kTerminalFatal)));

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TraceErrorCode::TraceErrorDisableAllTracePoints);
    TracingRuntimeAttorney attorney{*unit_under_test_};
    EXPECT_FALSE(unit_under_test_->IsTracingEnabled());
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, TraceShmDataNOK_NoCachedFiledescriptor)
{
    RecordProperty("Verifies", "SCR-18200105, SCR-18222321, SCR-18398047, SCR-18172392");
    RecordProperty(
        "Description",
        "Checks whether the right Trace call is done for data residing in shared-mem (SCR-18200105) and the right "
        "usage of ShmDataChunkList (SCR-18222321). Also checks the transmission data loss flag (SCR-18398047). "
        "Additionally it tests, that re-registration of a previous/once failed ShmObject registration is not tried."
        "(SCR-18172392), as there is no cached file descriptor, which means, that re-registering already failed. ");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // expect, that the binding specific tracing runtime doesn't have a ShmObjectHandle for the given identifier
    EXPECT_CALL(tracing_runtime_binding_mock_, GetShmObjectHandle(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(score::cpp::optional<analysis::tracing::ShmObjectHandle>{}));
    // then expect, that UuT calls GetCachedFileDescriptorForReregisteringShmObject() on binding specific tracing
    // runtime, which doesn't return any
    EXPECT_CALL(tracing_runtime_binding_mock_,
                GetCachedFileDescriptorForReregisteringShmObject(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(score::cpp::optional<std::pair<memory::shared::ISharedMemoryResource::FileDescriptor, void*>>{}));

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TraceErrorCode::TraceErrorDisableTracePointInstance);
    TracingRuntimeAttorney attorney{*unit_under_test_};
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    EXPECT_EQ(attorney.GetFailureCounter(), 0);
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, TraceShmDataNOK_GetShmRegionStartAddressFailedDeathTest)
{
    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    auto death_test_sequence = [this]() -> void {
        // expect, that a slot for a sample pointer can be found at the index trace_context_id
        EXPECT_CALL(tracing_runtime_binding_mock_, EmplaceTypeErasedSamplePtr(_, service_element_tracing_data_))
            .WillOnce(Return(trace_context_id_));
        analysis::tracing::ShmObjectHandle shm_object_handle{77};
        // and expect, that UuT gets successful a ShmObjectHandle for the service element instance from the binding
        // specific tracing runtime
        EXPECT_CALL(tracing_runtime_binding_mock_, GetShmObjectHandle(dummy_service_element_instance_identifier_view_))
            .WillOnce(Return(shm_object_handle));
        // but then the call by UuT to get the shm_start_address for the service element instance doesn't return an
        // address
        EXPECT_CALL(tracing_runtime_binding_mock_,
                    GetShmRegionStartAddress(dummy_service_element_instance_identifier_view_))
            .WillOnce(Return(score::cpp::optional<void*>{}));
        // when we call Trace on the UuT
        unit_under_test_->Trace(BindingType::kLoLa,
                                service_element_tracing_data_,
                                dummy_service_element_instance_identifier_view_,
                                trace_point_type_,
                                dummy_data_id_,
                                CreateDummySamplePtr(),
                                dummy_shm_data_ptr_,
                                dummy_shm_data_size_);
    };

    // we expect to die!
    EXPECT_DEATH(death_test_sequence(), ".*");
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, TraceShmDataNOK_NonRecoverableError)
{
    RecordProperty("Verifies", "SCR-18398059");
    RecordProperty(
        "Description",
        "Checks that after a non-recoverable error in Trace() call, the data-loss flag is set, the caller is notified, "
        "to abandon further trace-calls for the same trace-point-type and a log message mit severity warning is "
        "issued");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // expect, that all preconditions for a Trace call to the GenericTraceAPI are met
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    // expect, that UuT calls Trace on the GenericTraceAPI, which returns a non-recoverable error
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _, trace_context_id_))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kInvalidArgumentFatal)));

    // expect, that UuT frees sample_ptr on binding specific runtime as this trace call is lost and no trace-done
    // callback will happen, which frees the sample ptr.
    EXPECT_CALL(tracing_runtime_binding_mock_, ClearTypeErasedSamplePtr(trace_context_id_)).Times(1);

    // expect, that UuT sets data-loss-flag on binding specific runtime as this trace call is lost because of error
    EXPECT_CALL(tracing_runtime_binding_mock_, SetDataLossFlag(true)).Times(1);

    // capture stdout output during Trace() call.
    testing::internal::CaptureStdout();

    TracingRuntimeAttorney attorney{*unit_under_test_};
    auto previous_error_counter = attorney.GetFailureCounter();

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    // stop capture and get captured data.
    std::string log_output = testing::internal::GetCapturedStdout();
    const char log_warn_snippet[] = "log warn";
    const char text_snippet[] = "TracingRuntime: Disabling Tracing for ";
    // and expect, that the output contains a warning message (mw::log)
    auto first_offset = log_output.find(log_warn_snippet);
    EXPECT_TRUE(first_offset != log_output.npos);
    EXPECT_TRUE(log_output.find(text_snippet, first_offset) != log_output.npos);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TraceErrorCode::TraceErrorDisableTracePointInstance);

    // expect that tracing is still enabled afterward because non-recoverable error in Trace() just deactivates the
    // specific trace-point-instance
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    EXPECT_EQ(attorney.GetFailureCounter(), previous_error_counter + 1U);
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, TraceShmDataNOK_TerminalFatalError)
{
    RecordProperty("Verifies", "SCR-18398054");
    RecordProperty("Description",
                   "Checks that after a terminal fatal error in Trace() call, tracing is "
                   "completely disabled and a log message mit severity warning is issued");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // expect, that all preconditions for a Trace call to the GenericTraceAPI are met
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    // expect, that UuT calls Trace on the GenericTraceAPI, which returns a terminal fatal error
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _, trace_context_id_))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kTerminalFatal)));

    // expect, that UuT frees sample_ptr on binding specific runtime as this trace call is lost and no trace-done
    // callback will happen, which frees the sample ptr.
    EXPECT_CALL(tracing_runtime_binding_mock_, ClearTypeErasedSamplePtr(trace_context_id_)).Times(1);

    // capture stdout output during Trace() call.
    testing::internal::CaptureStdout();

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    // stop capture and get captured data.
    std::string log_output = testing::internal::GetCapturedStdout();
    const char log_warn_snippet[] = "log warn";
    const char text_snippet[] = "kTerminalFatal";
    // and expect, that the output contains a warning message (mw::log)
    auto first_offset = log_output.find(log_warn_snippet);
    EXPECT_TRUE(first_offset != log_output.npos);
    EXPECT_TRUE(log_output.find(text_snippet, first_offset) != log_output.npos);

    // expect, that there was an error
    EXPECT_FALSE(result.has_value());
    // and expect, that the error code is TraceErrorDisableAllTracePoints
    EXPECT_EQ(result.error(), TraceErrorCode::TraceErrorDisableAllTracePoints);
    // expect, that tracing is globally disabled
    TracingRuntimeAttorney attorney{*unit_under_test_};
    // expect that tracing is disabled afterwards because kTerminalFatal error in Trace() deactivates the tracing
    // completely
    EXPECT_FALSE(unit_under_test_->IsTracingEnabled());
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, TraceShmDataNOK_RecoverableError)
{
    RecordProperty("Verifies", "SCR-18200105, SCR-18222321, SCR-18398047, SCR-18398073");
    RecordProperty(
        "Description",
        "Checks whether the right Trace call is done for data residing in shared-mem (SCR-18200105) and the right "
        "usage of ShmDataChunkList (SCR-18222321). Also checks the transmission data loss flag (SCR-18398047). "
        "Furthermore checks, that in case of recoverable error the consecutive error counter gets incremented "
        "(SCR-18398073)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    TracingRuntimeAttorney attorney{*unit_under_test_};
    auto previous_failure_counter = attorney.GetFailureCounter();

    // expect, that all preconditions for a Trace call to the GenericTraceAPI are met
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    // expect, that UuT calls Trace on the GenericTraceAPI, which returns a non-recoverable error
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _, trace_context_id_))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kRingBufferFullRecoverable)));

    // expect, that UuT frees sample_ptr on binding specific runtime as this trace call is lost and no trace-done
    // callback will happen, which frees the sample ptr.
    EXPECT_CALL(tracing_runtime_binding_mock_, ClearTypeErasedSamplePtr(trace_context_id_)).Times(1);

    // expect, that UuT sets data-loss-flag on binding specific runtime as this trace call is lost because of error
    EXPECT_CALL(tracing_runtime_binding_mock_, SetDataLossFlag(true)).Times(1);

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    EXPECT_TRUE(result.has_value());
    // expect that tracing is enabled
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    // expect, that failure counter is incremented by one
    EXPECT_EQ(attorney.GetFailureCounter(), previous_failure_counter + 1);
}

TEST_P(TracingRuntimeTraceShmParamaterisedFixture, TraceShmDataNOK_ConsecutiveRecoverableErrors)
{
    RecordProperty("Verifies", "SCR-18200105, SCR-18222321, SCR-18398047, SCR-24726513");
    RecordProperty(
        "Description",
        "Checks whether the right Trace call is done for data residing in shared-mem (SCR-18200105) and the right "
        "usage of ShmDataChunkList (SCR-18222321). Also checks the transmission data loss flag (SCR-18398047) and "
        "that "
        "after a configured amount of consecutive Trace() error, tracing gets completely disabled (SCR-24726513)");
    RecordProperty("Priority", "1");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    TracingRuntimeAttorney attorney{*unit_under_test_};
    // expect that tracing is enabled
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    EXPECT_EQ(attorney.GetFailureCounter(), 0);

    const auto kRetries{10};
    static_assert(TracingRuntime::MAX_CONSECUTIVE_ACCEPTABLE_TRACE_FAILURES > kRetries);
    const auto kFailureCounterStart{TracingRuntime::MAX_CONSECUTIVE_ACCEPTABLE_TRACE_FAILURES - kRetries};
    attorney.SetFailureCounter(kFailureCounterStart);

    // expect, that all preconditions for a Trace call to the GenericTraceAPI are met
    SetupTracingRuntimeBindingMockForShmDataTraceCall();
    // expect, that UuT calls Trace on the GenericTraceAPI, which returns a recoverable error
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _, trace_context_id_))
        .Times(kRetries)
        .WillRepeatedly(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kRingBufferFullRecoverable)));
    // expect, that UuT frees sample_ptr on binding specific runtime as this trace call is lost and no trace-done
    // callback will happen, which frees the sample ptr.
    EXPECT_CALL(tracing_runtime_binding_mock_, ClearTypeErasedSamplePtr(trace_context_id_)).Times(kRetries);
    // expect, that UuT sets data-loss-flag on binding specific runtime as this trace call is lost because of
    // error
    EXPECT_CALL(tracing_runtime_binding_mock_, SetDataLossFlag(true)).Times(kRetries);

    for (unsigned i = 0; i < kRetries; i++)
    {
        EXPECT_EQ(attorney.GetFailureCounter(), kFailureCounterStart + i);
        // when we call Trace on the UuT
        auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                              service_element_tracing_data_,
                                              dummy_service_element_instance_identifier_view_,
                                              trace_point_type_,
                                              dummy_data_id_,
                                              CreateDummySamplePtr(),
                                              dummy_shm_data_ptr_,
                                              dummy_shm_data_size_);

        if (i < kRetries - 1)
        {
            EXPECT_TRUE(result.has_value());
            // expect that tracing is enabled
            EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
        }
    }
    // expect that tracing is disabled
    EXPECT_FALSE(unit_under_test_->IsTracingEnabled());
}

TEST_P(TracingRuntimeTraceInvalidTracePointTypeParamaterisedDeathTest,
       TracingShmDataWithInvalidTracePointTypeTerminates)
{
    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    // when we call Trace on the UuT with an invalid TracePointType
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit_under_test_->Trace(BindingType::kLoLa,
                                                       service_element_tracing_data_,
                                                       dummy_service_element_instance_identifier_view_,
                                                       trace_point_type_,
                                                       dummy_data_id_,
                                                       CreateDummySamplePtr(),
                                                       dummy_shm_data_ptr_,
                                                       dummy_shm_data_size_),
                 ".*");
}

TEST_P(TracingRuntimeTraceLocalParamaterisedFixture, CanConstructTracingRuntime)
{
    // When creating a tracingRuntime
    // Then a valid TracingRuntime is created
}

TEST_P(TracingRuntimeTraceLocalParamaterisedFixture, CallingTraceDispatchesToBinding)
{
    RecordProperty("Verifies", "SCR-18221771, SCR-18222516");
    RecordProperty("Description",
                   "Checks whether the right Trace call is done for local data (SCR-18221771). Also checks the "
                   "handling of LocalDataChunkLists (SCR-18222516)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    TracingRuntimeAttorney attorney{*unit_under_test_};
    auto previous_failure_counter = attorney.GetFailureCounter();

    analysis::tracing::LocalDataChunk root_chunk{kLocalDataPtr, kLocalDataSize};
    analysis::tracing::LocalDataChunkList expected_chunk_list{root_chunk};

    SetupTracingRuntimeBindingMockForLocalDataTraceCall();

    // and then expect, that UuT calls the GenericTraceAPI::Trace() call with local chunk list
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, Eq(ByRef(expected_chunk_list))))
        .WillOnce(Return(analysis::tracing::TraceResult{}));
    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          kEmptyDataId,
                                          kLocalDataPtr,
                                          kLocalDataSize);

    // expect, that there was no error
    EXPECT_TRUE(result.has_value());
    // expect, that tracing is still globally enabled
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    // expect, that failure counter is still the same
    EXPECT_EQ(attorney.GetFailureCounter(), previous_failure_counter);
}

TEST_P(TracingRuntimeTraceLocalParamaterisedFixture, TraceLocalData_RecoverableError)
{
    RecordProperty("Verifies", "SCR-18221771, SCR-18398047, SCR-18222516");
    RecordProperty(
        "Description",
        "Checks whether the right Trace call is done for local data (SCR-18221771). Also checks the transmission"
        "data loss flag (SCR-18398047) and handling of LocalDataChunkLists (SCR-18222516)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    TracingRuntimeAttorney attorney{*unit_under_test_};
    auto previous_failure_counter = attorney.GetFailureCounter();

    SetupTracingRuntimeBindingMockForLocalDataTraceCall();

    // and then expect, that UuT calls the GenericTraceAPI::Trace() call with local chunk list
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _))
        .WillOnce(Return(MakeUnexpected(analysis::tracing::ErrorCode::kNotEnoughMemoryRecoverable)));
    // and expect, that it calls binding specific tracing runtime SetDataLoss(true) after a failed call to
    // GenericTraceAPI::Trace()
    EXPECT_CALL(tracing_runtime_binding_mock_, SetDataLossFlag(true)).Times(1);
    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          kEmptyDataId,
                                          kLocalDataPtr,
                                          kLocalDataSize);

    // expect, that there was no error (as non-recoverable error would just lead to a returned error in case of
    // threshold reached)
    EXPECT_TRUE(result.has_value());
    // expect, that tracing is still globally enabled
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    // expect, that failure counter is incremented by one
    EXPECT_EQ(attorney.GetFailureCounter(), previous_failure_counter + 1);
}

TEST_P(TracingRuntimeTraceLocalParamaterisedFixture, TraceLocalData_NonRecoverableError)
{
    RecordProperty("Verifies", "SCR-18221771, SCR-18398047");
    RecordProperty(
        "Description",
        "Checks whether the right Trace call is done for local data (SCR-18221771). Also checks the transmission"
        "data loss flag (SCR-18398047)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    TracingRuntimeAttorney attorney{*unit_under_test_};
    auto previous_failure_counter = attorney.GetFailureCounter();

    analysis::tracing::LocalDataChunk root_chunk{kLocalDataPtr, kLocalDataSize};
    analysis::tracing::LocalDataChunkList expected_chunk_list{root_chunk};

    analysis::tracing::ServiceInstanceElement::EventIdType event_id = 42U;
    analysis::tracing::ServiceInstanceElement::VariantType variant{event_id};
    analysis::tracing::ServiceInstanceElement service_instance_element{25, 1, 0, 1, variant};

    // expect, that UuT calls GetDataLossFlag() on the binding specific tracing runtime to setup meta-info property
    EXPECT_CALL(tracing_runtime_binding_mock_, GetDataLossFlag()).WillOnce(Return(false));
    // and expect, that it calls binding specific tracing runtime to convert binding specific service element instance
    // identification into a independent/tracing specific one
    EXPECT_CALL(tracing_runtime_binding_mock_,
                ConvertToTracingServiceInstanceElement(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(service_instance_element));
    // and expect, that it calls binding specific tracing runtime to get its client id for call to GenericTraceAPI
    EXPECT_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillOnce(Return(trace_client_id_));
    // and then expect, that UuT calls the GenericTraceAPI::Trace() call with local chunk list
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, Eq(ByRef(expected_chunk_list))))
        .WillOnce(Return(MakeUnexpected(analysis::tracing::ErrorCode::kChannelCreationFailedFatal)));
    // and expect, that it calls binding specific tracing runtime SetDataLoss(true) after a failed call to
    // GenericTraceAPI::Trace()
    EXPECT_CALL(tracing_runtime_binding_mock_, SetDataLossFlag(true)).Times(1);
    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          kEmptyDataId,
                                          kLocalDataPtr,
                                          kLocalDataSize);

    // expect, that there was an error
    EXPECT_FALSE(result.has_value());

    EXPECT_EQ(result.error(), TraceErrorCode::TraceErrorDisableTracePointInstance);
    // expect, that tracing is still globally enabled
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    // expect, that failure counter is incremented by one
    EXPECT_EQ(attorney.GetFailureCounter(), previous_failure_counter + 1);
}

TEST_P(TracingRuntimeTraceLocalParamaterisedFixture, DisabledTracing_EarlyReturns)
{
    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    // and which has tracing disabled
    TracingRuntimeAttorney attorney{*unit_under_test_};
    attorney.SetTracingEnabled(false);

    // expect, that NO calls are done to the GenericTraceAPI
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(_, _, _, _)).Times(0);
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(_, _, _)).Times(0);
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterShmObject(_, testing::Matcher<const std::string&>())).Times(0);
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterShmObject(_, testing::Matcher<int>())).Times(0);
    EXPECT_CALL(*generic_trace_api_mock_.get(), UnregisterShmObject(_, _)).Times(0);

    // expect, that all calls to its public interface directly return (in case they have an error code return,
    // the code shall be TraceErrorDisableAllTracePoints)
    auto result1 = unit_under_test_->Trace(BindingType::kLoLa,
                                           dummy_service_element_instance_identifier_view_,
                                           trace_point_type_,
                                           score::cpp::optional<TracingRuntime::TracePointDataId>{},
                                           reinterpret_cast<void*>(static_cast<intptr_t>(500)),
                                           std::size_t{0});
    EXPECT_FALSE(result1.has_value());
    EXPECT_EQ(result1.error(), TraceErrorCode::TraceErrorDisableAllTracePoints);

    auto result2 = unit_under_test_->Trace(BindingType::kLoLa,
                                           service_element_tracing_data_,
                                           dummy_service_element_instance_identifier_view_,
                                           trace_point_type_,
                                           dummy_data_id_,
                                           CreateDummySamplePtr(),
                                           dummy_shm_data_ptr_,
                                           dummy_shm_data_size_);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error(), TraceErrorCode::TraceErrorDisableAllTracePoints);

    unit_under_test_->RegisterShmObject(BindingType::kLoLa,
                                        dummy_service_element_instance_identifier_view_,
                                        memory::shared::ISharedMemoryResource::FileDescriptor{1},
                                        reinterpret_cast<void*>(static_cast<uintptr_t>(777)));

    unit_under_test_->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_);
    unit_under_test_->SetDataLossFlag(BindingType::kLoLa);
}

TEST_P(TracingRuntimeTraceLocalParamaterisedFixture, TraceLocalData_FatalError)
{
    RecordProperty("Verifies", "SCR-18398054");
    RecordProperty("Description",
                   "Checks that after a terminal fatal error in Trace() call, tracing is "
                   "completely disabled and a log message mit severity warning is issued");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    TracingRuntimeAttorney attorney{*unit_under_test_};

    analysis::tracing::LocalDataChunk root_chunk{kLocalDataPtr, kLocalDataSize};
    analysis::tracing::LocalDataChunkList expected_chunk_list{root_chunk};

    analysis::tracing::ServiceInstanceElement::EventIdType event_id = 42U;
    analysis::tracing::ServiceInstanceElement::VariantType variant{event_id};
    analysis::tracing::ServiceInstanceElement service_instance_element{25, 1, 0, 1, variant};

    // expect, that UuT calls GetDataLossFlag() on the binding specific tracing runtime to setup meta-info property
    EXPECT_CALL(tracing_runtime_binding_mock_, GetDataLossFlag()).WillOnce(Return(false));
    // and expect, that it calls binding specific tracing runtime to convert binding specific service element instance
    // identification into a independent/tracing specific one
    EXPECT_CALL(tracing_runtime_binding_mock_,
                ConvertToTracingServiceInstanceElement(dummy_service_element_instance_identifier_view_))
        .WillOnce(Return(service_instance_element));
    // and expect, that it calls binding specific tracing runtime to get its client id for call to GenericTraceAPI
    EXPECT_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillOnce(Return(trace_client_id_));
    // and then expect, that UuT calls the GenericTraceAPI::Trace() call with local chunk list (which returns fatal
    // error)
    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, Eq(ByRef(expected_chunk_list))))
        .WillOnce(Return(MakeUnexpected(analysis::tracing::ErrorCode::kTerminalFatal)));
    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type_,
                                          kEmptyDataId,
                                          kLocalDataPtr,
                                          kLocalDataSize);

    // expect, that there was an error
    EXPECT_FALSE(result.has_value());
    // and expect, that the error code is TraceErrorDisableAllTracePoints
    EXPECT_EQ(result.error(), TraceErrorCode::TraceErrorDisableAllTracePoints);
    // expect, that tracing is globally disabled
    EXPECT_FALSE(unit_under_test_->IsTracingEnabled());
}

TEST_P(TracingRuntimeTraceInvalidTracePointTypeParamaterisedDeathTest,
       TracingLocalDataWithInvalidTracePointTypeTerminates)
{
    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    SetupTracingRuntimeBindingMockForLocalDataTraceCall();

    // When calling Trace with an invalid TracePointType
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit_under_test_->Trace(BindingType::kLoLa,
                                                       dummy_service_element_instance_identifier_view_,
                                                       trace_point_type_,
                                                       kEmptyDataId,
                                                       kLocalDataPtr,
                                                       kLocalDataSize),
                 ".*");
}

struct MetaInfoTestData
{
    TracingRuntime::TracePointType trace_point_type;
    analysis::tracing::TracePointType analysis_trace_point_type;
};
class TracingRuntimeMetaInfoParamaterisedFixture : public TracingRuntimeTraceFixture,
                                                   public ::testing::WithParamInterface<MetaInfoTestData>
{
};

using TracingRuntimeShmMetaInfoFixture = TracingRuntimeMetaInfoParamaterisedFixture;
INSTANTIATE_TEST_CASE_P(TracingRuntimeShmMetaInfoFixture,
                        TracingRuntimeShmMetaInfoFixture,
                        ::testing::Values(MetaInfoTestData{SkeletonEventTracePointType::SEND,
                                                           analysis::tracing::TracePointType::SKEL_EVENT_SND},
                                          MetaInfoTestData{SkeletonEventTracePointType::SEND_WITH_ALLOCATE,
                                                           analysis::tracing::TracePointType::SKEL_EVENT_SND_A},
                                          MetaInfoTestData{SkeletonFieldTracePointType::UPDATE,
                                                           analysis::tracing::TracePointType::SKEL_FIELD_UPD},
                                          MetaInfoTestData{SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE,
                                                           analysis::tracing::TracePointType::SKEL_FIELD_UPD_A}));

using TracingRuntimeLocalMetaInfoFixture = TracingRuntimeMetaInfoParamaterisedFixture;
INSTANTIATE_TEST_CASE_P(TracingRuntimeLocalMetaInfoFixture,
                        TracingRuntimeLocalMetaInfoFixture,
                        ::testing::Values(MetaInfoTestData{ProxyEventTracePointType::GET_NEW_SAMPLES,
                                                           analysis::tracing::TracePointType::PROXY_EVENT_GET_SAMPLES},
                                          MetaInfoTestData{ProxyEventTracePointType::SUBSCRIBE,
                                                           analysis::tracing::TracePointType::PROXY_EVENT_SUB},
                                          MetaInfoTestData{ProxyFieldTracePointType::GET_NEW_SAMPLES,
                                                           analysis::tracing::TracePointType::PROXY_FIELD_GET_SAMPLES},
                                          MetaInfoTestData{ProxyFieldTracePointType::SUBSCRIBE,
                                                           analysis::tracing::TracePointType::PROXY_FIELD_SUB}));

TEST_P(TracingRuntimeShmMetaInfoFixture, ShmTraceCallMetaInfoContainsAraComMetaInfo)
{
    RecordProperty("Verifies", "SCR-18200119");
    RecordProperty("Description",
                   "Checks that the meta_info type used in Shm Trace calls are set to the variant AraComMetaInfo");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto meta_info_test_data = GetParam();

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _, trace_context_id_))
        .WillOnce(WithArg<1>(Invoke([](const auto& meta_info) -> analysis::tracing::TraceResult {
            // Then the meta_info is set to the variant AraComMetaInfo
            const auto* const ara_com_meta_info = score::cpp::get_if<analysis::tracing::AraComMetaInfo>(&meta_info);
            EXPECT_NE(ara_com_meta_info, nullptr);

            return analysis::tracing::TraceResult{};
        })));

    // when we call Trace on the UuT
    score::cpp::ignore = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          meta_info_test_data.trace_point_type,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);
}

TEST_P(TracingRuntimeShmMetaInfoFixture,
       ShmTraceCallMetaInfoPropertiesContainsCorrectTracePointTypeAndServiceInstanceElement)
{
    RecordProperty("Verifies", "SCR-18200709");
    RecordProperty("Description",
                   "Checks that the meta_info properties used in Shm Trace calls have the correct TracePointType and "
                   "ServiceInstanceElement.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto meta_info_test_data = GetParam();

    const auto expected_trace_point_type{meta_info_test_data.analysis_trace_point_type};
    const auto expected_lola_trace_point_type{meta_info_test_data.trace_point_type};

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _, trace_context_id_))
        .WillOnce(
            WithArg<1>(Invoke([&expected_trace_point_type](const auto& meta_info) -> analysis::tracing::TraceResult {
                const auto* const ara_com_meta_info = score::cpp::get_if<analysis::tracing::AraComMetaInfo>(&meta_info);
                EXPECT_NE(ara_com_meta_info, nullptr);

                // Then the meta_info properties contain the correct TracePointType and ServiceInstanceElement
                const auto [actual_trace_point_type, actual_service_instance_element] =
                    ara_com_meta_info->properties_.trace_point_id_;

                EXPECT_EQ(actual_trace_point_type, expected_trace_point_type);
                EXPECT_EQ(actual_service_instance_element, kServiceInstanceElement);

                return analysis::tracing::TraceResult{};
            })));

    // when we call Trace on the UuT
    score::cpp::ignore = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          expected_lola_trace_point_type,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);
}

TEST_P(TracingRuntimeLocalMetaInfoFixture, LocalTraceCallMetaInfoContainsAraComMetaInfo)
{
    RecordProperty("Verifies", "SCR-18200119");
    RecordProperty("Description",
                   "Checks that the meta_info type used in local Trace calls are set to the variant AraComMetaInfo");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto meta_info_test_data = GetParam();

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _))
        .WillOnce(WithArg<1>(Invoke([](const auto& meta_info) -> analysis::tracing::TraceResult {
            // Then the meta_info is set to the variant AraComMetaInfo
            const auto* const ara_com_meta_info = score::cpp::get_if<analysis::tracing::AraComMetaInfo>(&meta_info);
            EXPECT_NE(ara_com_meta_info, nullptr);

            return analysis::tracing::TraceResult{};
        })));

    // when we call Trace on the UuT
    score::cpp::ignore = unit_under_test_->Trace(BindingType::kLoLa,
                                          dummy_service_element_instance_identifier_view_,
                                          meta_info_test_data.trace_point_type,
                                          kEmptyDataId,
                                          kLocalDataPtr,
                                          kLocalDataSize);
}

TEST_P(TracingRuntimeLocalMetaInfoFixture,
       LocalTraceCallMetaInfoPropertiesContainsCorrectTracePointTypeAndServiceInstanceElement)
{
    RecordProperty("Verifies", "SCR-18200709");
    RecordProperty("Description",
                   "Checks that the meta_info properties used in local Trace calls have the correct TracePointType and "
                   "ServiceInstanceElement.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto meta_info_test_data = GetParam();

    const auto expected_trace_point_type{meta_info_test_data.analysis_trace_point_type};
    const auto expected_lola_trace_point_type{meta_info_test_data.trace_point_type};

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa
    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _))
        .WillOnce(
            WithArg<1>(Invoke([&expected_trace_point_type](const auto& meta_info) -> analysis::tracing::TraceResult {
                // Then the meta_info is set to the variant AraComMetaInfo
                const auto* const ara_com_meta_info = score::cpp::get_if<analysis::tracing::AraComMetaInfo>(&meta_info);
                EXPECT_NE(ara_com_meta_info, nullptr);

                // Then the meta_info properties contain the correct TracePointType and ServiceInstanceElement
                const auto [actual_trace_point_type, actual_service_instance_element] =
                    ara_com_meta_info->properties_.trace_point_id_;

                EXPECT_EQ(actual_trace_point_type, expected_trace_point_type);
                EXPECT_EQ(actual_service_instance_element, kServiceInstanceElement);

                return analysis::tracing::TraceResult{};
            })));

    // when we call Trace on the UuT
    score::cpp::ignore = unit_under_test_->Trace(BindingType::kLoLa,
                                          dummy_service_element_instance_identifier_view_,
                                          expected_lola_trace_point_type,
                                          kEmptyDataId,
                                          kLocalDataPtr,
                                          kLocalDataSize);
}

struct DataLossFlagTestData
{
    TracingRuntime::TracePointType trace_point_type;
    bool data_loss_flag;
};
class TracingRuntimeTraceDataLossFlagParameterisedFixture : public TracingRuntimeTraceFixture,
                                                            public ::testing::WithParamInterface<DataLossFlagTestData>
{
};

INSTANTIATE_TEST_SUITE_P(TracingRuntimeTraceDataLossFlagParameterisedFixture,
                         TracingRuntimeTraceDataLossFlagParameterisedFixture,
                         ::testing::Values(DataLossFlagTestData{SkeletonEventTracePointType::SEND, true},
                                           DataLossFlagTestData{SkeletonEventTracePointType::SEND, false},
                                           DataLossFlagTestData{SkeletonFieldTracePointType::UPDATE, true},
                                           DataLossFlagTestData{SkeletonFieldTracePointType::UPDATE, false}));

TEST_P(TracingRuntimeTraceDataLossFlagParameterisedFixture, CallingShmTraceWillTransmitCurrentValueOfDataLossFlag)
{
    RecordProperty("Verifies", "SCR-18398047, SCR-18398043");
    RecordProperty("Description",
                   "Checks the transmission data loss flag with Shm Trace call (SCR-18398047). The value of the data "
                   "loss flag will be retrieved from the binding TracingRuntime (SCR-18398043).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto data_loss_flag_test_data = GetParam();
    const auto trace_point_type = data_loss_flag_test_data.trace_point_type;
    const auto data_loss_flag = data_loss_flag_test_data.data_loss_flag;

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    SetupTracingRuntimeBindingMockForShmDataTraceCall();

    EXPECT_CALL(tracing_runtime_binding_mock_, GetDataLossFlag()).WillOnce(Return(data_loss_flag));

    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _, trace_context_id_))
        .WillOnce(WithArg<1>(Invoke([data_loss_flag](const auto& meta_info) -> analysis::tracing::TraceResult {
            const auto ara_com_meta_info = score::cpp::get<analysis::tracing::AraComMetaInfo>(meta_info);
            const auto transmitted_data_loss_value_bitset = ara_com_meta_info.trace_status_;

            if (data_loss_flag)
            {
                EXPECT_TRUE(transmitted_data_loss_value_bitset.any());
            }
            else
            {
                EXPECT_FALSE(transmitted_data_loss_value_bitset.any());
            }
            return analysis::tracing::TraceResult{};
        })));

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          service_element_tracing_data_,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type,
                                          dummy_data_id_,
                                          CreateDummySamplePtr(),
                                          dummy_shm_data_ptr_,
                                          dummy_shm_data_size_);

    EXPECT_TRUE(result.has_value());
}

TEST_P(TracingRuntimeTraceDataLossFlagParameterisedFixture, CallingLocalTraceWillTransmitCurrentValueOfDataLossFlag)
{
    RecordProperty("Verifies", "SCR-18398047, SCR-18398043");
    RecordProperty("Description",
                   "Checks the transmission data loss flag with local Trace call (SCR-18398047). The value of the data "
                   "loss flag will be retrieved from the binding TracingRuntime (SCR-18398043).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const auto data_loss_flag_test_data = GetParam();
    const auto trace_point_type = data_loss_flag_test_data.trace_point_type;
    const auto data_loss_flag = data_loss_flag_test_data.data_loss_flag;

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    SetupTracingRuntimeBindingMockForLocalDataTraceCall();

    EXPECT_CALL(tracing_runtime_binding_mock_, GetDataLossFlag()).WillOnce(Return(data_loss_flag));

    EXPECT_CALL(*generic_trace_api_mock_.get(), Trace(trace_client_id_, _, _))
        .WillOnce(WithArg<1>(Invoke([data_loss_flag](const auto& meta_info) -> analysis::tracing::TraceResult {
            const auto ara_com_meta_info = score::cpp::get<analysis::tracing::AraComMetaInfo>(meta_info);
            const auto transmitted_data_loss_value_bitset = ara_com_meta_info.trace_status_;

            if (data_loss_flag)
            {
                EXPECT_TRUE(transmitted_data_loss_value_bitset.any());
            }
            else
            {
                EXPECT_FALSE(transmitted_data_loss_value_bitset.any());
            }
            return analysis::tracing::TraceResult{};
        })));

    // when we call Trace on the UuT
    auto result = unit_under_test_->Trace(BindingType::kLoLa,
                                          dummy_service_element_instance_identifier_view_,
                                          trace_point_type,
                                          kEmptyDataId,
                                          kLocalDataPtr,
                                          kLocalDataSize);

    // expect, that there was no error
    EXPECT_TRUE(result.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
