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
#include "score/mw/com/impl/tracing/tracing_runtime.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/mock_binding/tracing/tracing_runtime.h"
#include "score/mw/com/impl/tracing/tracing_test_resources.h"

#include "score/analysis/tracing/generic_trace_library/interface_types/error_code/error_code.h"
#include "score/analysis/tracing/generic_trace_library/mock/trace_library_mock.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl::tracing
{

namespace
{
constexpr std::string_view kDummyServiceTypeName{"my_service_type"};
constexpr std::string_view kDummyElementName{"my_event"};
constexpr std::string_view kInstanceSpecifier{"/my_service_type_port"};

constexpr std::uint8_t kNumberOfIpcTracingSlots{1U};
constexpr ServiceElementTracingData kServiceElementTracingData{0U, kNumberOfIpcTracingSlots};

const memory::shared::ISharedMemoryResource::FileDescriptor kShmFileDescriptor{1};
void* const kShmObjectStartAddress{reinterpret_cast<void*>(static_cast<uintptr_t>(777))};
const analysis::tracing::ShmObjectHandle kGenericTraceApiShmHandle{5};

using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::WithArg;

class TracingRuntimeFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        generic_trace_api_mock_ = std::make_unique<score::analysis::tracing::TraceLibraryMock>();
        std::unordered_map<BindingType, ITracingRuntimeBinding*> tracing_runtime_binding_map{};
        tracing_runtime_binding_map.emplace(registered_binding_type_, &tracing_runtime_binding_mock_);
        EXPECT_CALL(tracing_runtime_binding_mock_, RegisterWithGenericTraceApi()).WillOnce(Return(true));

        unit_under_test_ = std::make_unique<TracingRuntime>(std::move(tracing_runtime_binding_map));
        ASSERT_TRUE(unit_under_test_);
        TracingRuntimeAttorney attorney{*unit_under_test_};
        EXPECT_EQ(attorney.GetFailureCounter(), 0U);
        EXPECT_TRUE(unit_under_test_->IsTracingEnabled());

        ON_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillByDefault(Return(trace_client_id_));
    };

    TracingRuntimeFixture& WithARegisteredShmObjectHandle()
    {
        unit_under_test_->RegisterShmObject(BindingType::kLoLa,
                                            dummy_service_element_instance_identifier_view_,
                                            kShmFileDescriptor,
                                            kShmObjectStartAddress);

        ON_CALL(tracing_runtime_binding_mock_, GetShmObjectHandle(dummy_service_element_instance_identifier_view_))
            .WillByDefault(Return(kGenericTraceApiShmHandle));
        return *this;
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
    analysis::tracing::TraceClientId trace_client_id_{1};
    static constexpr BindingType registered_binding_type_{BindingType::kLoLa};
};

TEST(TracingRuntimeMove, MoveAssign)
{
    mock_binding::TracingRuntime tracing_runtime_binding_mock_1;
    mock_binding::TracingRuntime tracing_runtime_binding_mock_2;
    mock_binding::TracingRuntime tracing_runtime_binding_mock_3;
    std::unordered_map<BindingType, ITracingRuntimeBinding*> tracing_runtime_binding_map_1;
    tracing_runtime_binding_map_1.emplace(BindingType::kLoLa, &tracing_runtime_binding_mock_1);
    tracing_runtime_binding_map_1.emplace(BindingType::kFake, &tracing_runtime_binding_mock_2);
    std::unordered_map<BindingType, ITracingRuntimeBinding*> tracing_runtime_binding_map_2;
    tracing_runtime_binding_map_2.emplace(BindingType::kLoLa, &tracing_runtime_binding_mock_3);

    TracingRuntime runtime_1(std::move(tracing_runtime_binding_map_1));
    TracingRuntime runtime_2(std::move(tracing_runtime_binding_map_2));

    TracingRuntimeAttorney attorney_1{runtime_1};
    TracingRuntimeAttorney attorney_2{runtime_2};
    attorney_2.SetTracingEnabled(false);
    attorney_2.SetFailureCounter(42);

    runtime_1 = std::move(runtime_2);
    EXPECT_FALSE(runtime_1.IsTracingEnabled());
    EXPECT_EQ(attorney_1.GetFailureCounter(), 42);
    EXPECT_EQ(attorney_1.GetTracingRuntimeBindings().size(), 1);
}

TEST(TracingRuntimeMove, MoveConstruct)
{
    mock_binding::TracingRuntime tracing_runtime_binding_mock_1;
    mock_binding::TracingRuntime tracing_runtime_binding_mock_2;

    std::unordered_map<BindingType, ITracingRuntimeBinding*> tracing_runtime_binding_map_1;
    tracing_runtime_binding_map_1.emplace(BindingType::kLoLa, &tracing_runtime_binding_mock_1);
    tracing_runtime_binding_map_1.emplace(BindingType::kFake, &tracing_runtime_binding_mock_2);

    TracingRuntime runtime_1(std::move(tracing_runtime_binding_map_1));
    TracingRuntimeAttorney attorney_1{runtime_1};
    attorney_1.SetTracingEnabled(false);
    attorney_1.SetFailureCounter(42);

    TracingRuntime runtime_2(std::move(runtime_1));
    TracingRuntimeAttorney attorney_2{runtime_2};

    EXPECT_FALSE(runtime_2.IsTracingEnabled());
    EXPECT_EQ(attorney_2.GetFailureCounter(), 42);
}

TEST(TracingRuntime, TracingRuntimeTraceWillReceivePointerToConstShmData)
{
    RecordProperty("Verifies", "SCR-32156767");
    RecordProperty(
        "Description",
        "Checks that the pointer to the shared memory data to be traced is passed to the TracingRuntime::Trace as "
        "a pointer to const.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using ShmPointerType = const void*;

    // Check that ShmPointerType is the type that is passed to TracingRuntime::Trace
    constexpr auto trace_shm_signature = static_cast<score::ResultBlank (
        score::mw::com::impl::tracing::TracingRuntime::*)(BindingType,
                                                          ServiceElementInstanceIdentifierView,
                                                          ITracingRuntime::TracePointType,
                                                          score::cpp::optional<ITracingRuntime::TracePointDataId>,
                                                          ShmPointerType,
                                                          std::size_t)>(&TracingRuntime::Trace);
    static_assert(std::is_member_function_pointer_v<decltype(trace_shm_signature)>,
                  "shm_trace_signature is not a method of TracingRuntime.");

    // Check that ShmPointerType is a pointer to const
    static_assert(std::is_const_v<std::remove_pointer_t<ShmPointerType>>,
                  "Pointer to shared memory is not a pointer to const.");
}

TEST(TracingRuntimeDisable, TraceClientRegistrationFails)
{
    RecordProperty("Verifies", "SCR-18159752");
    RecordProperty("Description",
                   "Checks whether the binding specific tracing runtimes are triggered to register themselves as "
                   "clients and that a failure even with one client leads to global disabling of tracing.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given two binding specific tracing runtimes
    mock_binding::TracingRuntime tracing_runtime_binding_mock_1;
    mock_binding::TracingRuntime tracing_runtime_binding_mock_2;
    std::unordered_map<BindingType, ITracingRuntimeBinding*> tracing_runtime_binding_map;
    tracing_runtime_binding_map.emplace(BindingType::kLoLa, &tracing_runtime_binding_mock_1);
    tracing_runtime_binding_map.emplace(BindingType::kFake, &tracing_runtime_binding_mock_2);

    // expect, that one of those binding specific tracing runtimes registers successfully with the GenericTraceAPI
    EXPECT_CALL(tracing_runtime_binding_mock_1, RegisterWithGenericTraceApi()).WillOnce(Return(true));
    // expect, that the other of those binding specific tracing runtimes registers NOT successfully with the
    // GenericTraceAPI
    EXPECT_CALL(tracing_runtime_binding_mock_2, RegisterWithGenericTraceApi()).WillOnce(Return(false));

    // when the tracing runtime is created with those two binding specific runtimes
    TracingRuntime runtime(std::move(tracing_runtime_binding_map));

    // then expect, that tracing is globally disabled.
    EXPECT_FALSE(runtime.IsTracingEnabled());
}

TEST_F(TracingRuntimeFixture, CanCreateTracingRuntime)
{
    // When creating a tracingRuntime
    // Then a valid TracingRuntime is created
}

TEST_F(TracingRuntimeFixture, SetDataLossFlag)
{
    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // expect, that SetDataLossFlag(true) is called on the ITracingRuntimeBinding
    EXPECT_CALL(tracing_runtime_binding_mock_, SetDataLossFlag(true)).Times(1);

    // when we call SetDataLossFlag(BindingType::kLoLa) on the UuT
    unit_under_test_->SetDataLossFlag(BindingType::kLoLa);
}

using TracingRuntimeRegisterShmObjectFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeRegisterShmObjectFixture, RegisterShmObjectOK)
{
    RecordProperty("Verifies", "SCR-18166404");
    RecordProperty("Description", "Verifies, that the correct API from GenericTraceAPI gets called.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // expect, that UuT calls GetTraceClientId() on the binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillOnce(Return(trace_client_id_));
    // and that it calls RegisterShmObject() on the GenericTraceAPI, which returns kGenericTraceApiShmHandle as the
    // translated kShmFileDescriptor
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterShmObject(trace_client_id_, kShmFileDescriptor))
        .WillOnce(Return(analysis::tracing::RegisterSharedMemoryObjectResult{kGenericTraceApiShmHandle}));
    // and that UuT calls RegisterShmObject on the binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_,
                RegisterShmObject(
                    dummy_service_element_instance_identifier_view_, kGenericTraceApiShmHandle, kShmObjectStartAddress))
        .Times(1);

    // when calling RegisterShmObject on the UuT.
    unit_under_test_->RegisterShmObject(BindingType::kLoLa,
                                        dummy_service_element_instance_identifier_view_,
                                        kShmFileDescriptor,
                                        kShmObjectStartAddress);

    // expect, that afterwards tracing is still enabled and the failure counter is 0.
    TracingRuntimeAttorney attorney{*unit_under_test_};
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    EXPECT_EQ(attorney.GetFailureCounter(), 0);
}

TEST_F(TracingRuntimeRegisterShmObjectFixture, RegisterShmObjectNOK_unrecoverable)
{
    RecordProperty("Verifies", "SCR-18166404, SCR-18172406");
    RecordProperty("Description",
                   "Verifies, that the correct API from GenericTraceAPI gets called and that in case of an "
                   "unrecoverable error no registration-retry logic is set up.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // expect, that UuT calls GetTraceClientId() on the binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillOnce(Return(trace_client_id_));
    // and that it calls RegisterShmObject() on the GenericTraceAPI, which returns an unrecoverable error
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterShmObject(trace_client_id_, kShmFileDescriptor))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kInvalidArgumentFatal)));

    // when calling RegisterShmObject on the UuT.
    unit_under_test_->RegisterShmObject(BindingType::kLoLa,
                                        dummy_service_element_instance_identifier_view_,
                                        kShmFileDescriptor,
                                        kShmObjectStartAddress);

    // expect, that afterwards tracing is still enabled.
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
}

TEST_F(TracingRuntimeRegisterShmObjectFixture, RegisterShmObjectNOK_FatalError)
{
    RecordProperty("Verifies", "SCR-18398054");
    RecordProperty("Description",
                   "Checks that after a terminal fatal error in RegisterShmObject() call, tracing is "
                   "completely disabled and a log message mit severity warning is "
                   "issued");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // expect, that UuT calls GetTraceClientId() on the binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillOnce(Return(trace_client_id_));
    // and that it calls RegisterShmObject() on the GenericTraceAPI, which returns an unrecoverable error
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterShmObject(trace_client_id_, kShmFileDescriptor))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kTerminalFatal)));

    // when calling RegisterShmObject on the UuT.
    unit_under_test_->RegisterShmObject(BindingType::kLoLa,
                                        dummy_service_element_instance_identifier_view_,
                                        kShmFileDescriptor,
                                        kShmObjectStartAddress);

    // expect, that afterwards tracing is disabled.
    EXPECT_FALSE(unit_under_test_->IsTracingEnabled());
}

TEST_F(TracingRuntimeRegisterShmObjectFixture, RegisterShmObjectNOK_recoverable)
{
    RecordProperty("Verifies", "SCR-18166404, SCR-18172392");
    RecordProperty("Description",
                   "Verifies, that the correct API from GenericTraceAPI gets called and that in case of an "
                   "recoverable error registration-retry logic is set up.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // expect, that UuT calls GetTraceClientId() on the binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_, GetTraceClientId()).WillOnce(Return(trace_client_id_));
    // and that it calls RegisterShmObject() on the GenericTraceAPI, which returns a recoverable error
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterShmObject(trace_client_id_, kShmFileDescriptor))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kMessageSendFailedRecoverable)));
    // and that UuT calls CacheFileDescriptorForReregisteringShmObject() on the binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_,
                CacheFileDescriptorForReregisteringShmObject(
                    dummy_service_element_instance_identifier_view_, kShmFileDescriptor, kShmObjectStartAddress))
        .Times(1);

    // when calling RegisterShmObject on the UuT.
    unit_under_test_->RegisterShmObject(BindingType::kLoLa,
                                        dummy_service_element_instance_identifier_view_,
                                        kShmFileDescriptor,
                                        kShmObjectStartAddress);

    // expect, that afterwards tracing is still enabled.
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
}

using TracingRuntimeUnregisterShmObjectFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeUnregisterShmObjectFixture, UnregisterShmObjectDispatchesToTracingRuntimeBinding)
{
    // Given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa which has registered a
    // shm object
    WithARegisteredShmObjectHandle();

    // Expecting that UuT calls UnregisterShmObject on the binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_, UnregisterShmObject(dummy_service_element_instance_identifier_view_));

    // When calling UnregisterShmObject on the UuT.
    unit_under_test_->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_);
}

TEST_F(TracingRuntimeUnregisterShmObjectFixture, UnregisterShmObjectDispatchesToGenericTraceApiBinding)
{
    // Given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa which has registered a
    // shm object
    WithARegisteredShmObjectHandle();

    // Expecting that UuT calls UnregisterShmObject() on the GenericTraceAPI
    EXPECT_CALL(*generic_trace_api_mock_.get(), UnregisterShmObject(trace_client_id_, kGenericTraceApiShmHandle));

    // When calling UnregisterShmObject on the UuT.
    unit_under_test_->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_);
}

TEST_F(TracingRuntimeUnregisterShmObjectFixture, UnregisterShmObjectDoesNotDispatchToAnyBindingsWhenTracingIsDisabled)
{
    // Given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa which has tracing
    // disabled
    unit_under_test_->DisableTracing();

    // Expecting that UuT does not call UnregisterShmObject() on the GenericTraceAPI or binding specific tracing runtime
    EXPECT_CALL(tracing_runtime_binding_mock_, UnregisterShmObject(dummy_service_element_instance_identifier_view_))
        .Times(0);
    EXPECT_CALL(*generic_trace_api_mock_.get(), UnregisterShmObject(trace_client_id_, kGenericTraceApiShmHandle))
        .Times(0);

    // When calling UnregisterShmObject on the UuT.
    unit_under_test_->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_);
}

TEST_F(TracingRuntimeUnregisterShmObjectFixture,
       UnregisterShmObjectDisablesTracingWhenBindingReturnsATerminalFatalError)
{
    // Given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa which has registered a
    // shm object
    WithARegisteredShmObjectHandle();

    // and that it calls UnregisterShmObject() on the GenericTraceAPI which returns a terminal fatal error
    ON_CALL(*generic_trace_api_mock_.get(), UnregisterShmObject(trace_client_id_, kGenericTraceApiShmHandle))
        .WillByDefault(Return(MakeUnexpected(analysis::tracing::ErrorCode::kTerminalFatal)));

    // When calling UnregisterShmObject on the UuT.
    unit_under_test_->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_);

    // Then tracing is disabled
    EXPECT_FALSE(unit_under_test_->IsTracingEnabled());
}

TEST_F(TracingRuntimeUnregisterShmObjectFixture,
       UnregisterShmObjectDoesNotDisableTracingOrIncrementFailureCounterWhenBindingReturnsRecoverableError)
{
    // Given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa which has registered a
    // shm object
    WithARegisteredShmObjectHandle();

    // and that it calls UnregisterShmObject() on the GenericTraceAPI which returns a recoverable error
    ON_CALL(*generic_trace_api_mock_.get(), UnregisterShmObject(trace_client_id_, kGenericTraceApiShmHandle))
        .WillByDefault(Return(MakeUnexpected(analysis::tracing::ErrorCode::kNotEnoughMemoryRecoverable)));

    // When calling UnregisterShmObject on the UuT.
    unit_under_test_->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_);

    // Then tracing is still enabled and the failure counter is 0.
    TracingRuntimeAttorney attorney{*unit_under_test_};
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    EXPECT_EQ(attorney.GetFailureCounter(), 0);
}

TEST_F(TracingRuntimeUnregisterShmObjectFixture,
       UnregisterShmObjectDoesNotDisableTracingOrIncrementFailureCounterWhenBindingReturnsNonRecoverableError)
{
    // Given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa which has registered a
    // shm object
    WithARegisteredShmObjectHandle();

    // and that it calls UnregisterShmObject() on the GenericTraceAPI which returns a non-recoverable error
    ON_CALL(*generic_trace_api_mock_.get(), UnregisterShmObject(trace_client_id_, kGenericTraceApiShmHandle))
        .WillByDefault(Return(MakeUnexpected(analysis::tracing::ErrorCode::kDaemonNotConnectedFatal)));

    // When calling UnregisterShmObject on the UuT.
    unit_under_test_->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_);

    // Then tracing is still enabled and the failure counter is 0.
    TracingRuntimeAttorney attorney{*unit_under_test_};
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    EXPECT_EQ(attorney.GetFailureCounter(), 0);
}

TEST_F(TracingRuntimeUnregisterShmObjectFixture,
       UnregisterShmObjectWillClearCacheFileDescriptorsWhenShmObjectHandleNotFound)
{
    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // and that UuT calls GetShmObjectHandle on the binding specific tracing runtime, which returns an empty optional
    ON_CALL(tracing_runtime_binding_mock_, GetShmObjectHandle(dummy_service_element_instance_identifier_view_))
        .WillByDefault(Return(score::cpp::optional<analysis::tracing::ShmObjectHandle>{}));

    // Expecting that UuT calls ClearCachedFileDescriptorForReregisteringShmObject on the binding specific tracing
    // runtime
    EXPECT_CALL(tracing_runtime_binding_mock_,
                ClearCachedFileDescriptorForReregisteringShmObject(dummy_service_element_instance_identifier_view_));

    // when calling UnregisterShmObject on the UuT.
    unit_under_test_->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_);
}

TEST_F(TracingRuntimeUnregisterShmObjectFixture,
       UnregisterShmObjectWillNotDisableTracingOrIncrementFailureCounterWhenShmObjectHandleNotFound)
{
    // given a UuT which delegates to a mock ITracingRuntimeBinding in case of BindingType::kLoLa

    // and that UuT calls GetShmObjectHandle on the binding specific tracing runtime, which returns an empty optional
    ON_CALL(tracing_runtime_binding_mock_, GetShmObjectHandle(dummy_service_element_instance_identifier_view_))
        .WillByDefault(Return(score::cpp::optional<analysis::tracing::ShmObjectHandle>{}));

    // when calling UnregisterShmObject on the UuT.
    unit_under_test_->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_);

    // Then tracing is still enabled and the failure counter is 0.
    TracingRuntimeAttorney attorney{*unit_under_test_};
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
    EXPECT_EQ(attorney.GetFailureCounter(), 0);
}

using TracingRuntimeUnregisterShmObjectDeathTest = TracingRuntimeUnregisterShmObjectFixture;
TEST_F(TracingRuntimeUnregisterShmObjectDeathTest, UnregisterShmObjectTerminatesWhenTracingRuntimeBindingCannotBeFound)
{
    // Given a TracingRuntime that is constructed without any tracing runtime bindings
    std::unordered_map<BindingType, ITracingRuntimeBinding*> tracing_runtime_binding_map{};
    auto unit_under_test = std::make_unique<TracingRuntime>(std::move(tracing_runtime_binding_map));

    // When calling UnregisterShmObject
    // Then the program terminates
    EXPECT_DEATH(
        unit_under_test->UnregisterShmObject(BindingType::kLoLa, dummy_service_element_instance_identifier_view_),
        ".*");
}

using TracingRuntimeRegisterServiceElementFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeRegisterServiceElementFixture, RegisterServiceElementDispatchesToTracingRuntimeBinding)
{
    // Given a TracingRuntime

    // Expecting that RegisterServiceElement will be called on the tracing runtime binding
    EXPECT_CALL(tracing_runtime_binding_mock_, RegisterServiceElement(kNumberOfIpcTracingSlots));

    // When calling RegisterServiceElement
    score::cpp::ignore = unit_under_test_->RegisterServiceElement(registered_binding_type_, kNumberOfIpcTracingSlots);
}

TEST_F(TracingRuntimeRegisterServiceElementFixture,
       CallingRegisterServiceElementReturnsTheServiceElementTracingDataFromTheBinding)
{
    // Given a TracingRuntime
    // and that RegisterServiceElement will be called on the tracing runtime binding
    ON_CALL(tracing_runtime_binding_mock_, RegisterServiceElement(_)).WillByDefault(Return(kServiceElementTracingData));

    // When calling RegisterServiceElement
    const auto actual_service_element_tracing_data =
        unit_under_test_->RegisterServiceElement(registered_binding_type_, kNumberOfIpcTracingSlots);

    // Then the service element tracing data returned by the binding is returned
    EXPECT_EQ(actual_service_element_tracing_data, kServiceElementTracingData);
}

using TracingRuntimeRegisterServiceElementDeathTest = TracingRuntimeRegisterServiceElementFixture;
TEST_F(TracingRuntimeRegisterServiceElementDeathTest,
       RegisterServiceElementTerminatesWhenTracingRuntimeBindingCannotBeFound)
{
    // Given a TracingRuntime that is constructed without any tracing runtime bindings
    std::unordered_map<BindingType, ITracingRuntimeBinding*> tracing_runtime_binding_map{};
    auto unit_under_test = std::make_unique<TracingRuntime>(std::move(tracing_runtime_binding_map));

    // When calling RegisterServiceElement
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore =
                     unit_under_test->RegisterServiceElement(registered_binding_type_, kNumberOfIpcTracingSlots),
                 ".*");
}

using TracingRuntimeDisableTracingFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeRegisterServiceElementFixture, TracingIsEnabledByDefault)
{
    // When constructing a TracingRuntime

    // Then tracing will be enabled by default
    EXPECT_TRUE(unit_under_test_->IsTracingEnabled());
}

TEST_F(TracingRuntimeRegisterServiceElementFixture, CallignDisableTracingWillDisableTracing)
{
    // Given a TracingRuntime

    // When calling DisableTracing
    unit_under_test_->DisableTracing();

    // Then tracing will be disabled
    EXPECT_FALSE(unit_under_test_->IsTracingEnabled());
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
