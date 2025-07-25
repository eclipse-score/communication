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
#include "score/mw/com/impl/bindings/lola/tracing/tracing_runtime.h"

#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"
#include "score/mw/com/impl/bindings/mock_binding/sample_ptr.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"

#include "score/analysis/tracing/common/interface_types/types.h"
#include "score/analysis/tracing/library/generic_trace_api/mocks/trace_library_mock.h"

#include "score/mw/com/impl/tracing/service_element_tracing_data.h"
#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace score::mw::com::impl::lola::tracing
{

using TestSampleType = std::uint16_t;

class TracingRuntimeAttorney
{
  public:
    TracingRuntimeAttorney(TracingRuntime& tracing_runtime) : tracing_runtime_{tracing_runtime} {}

    score::containers::DynamicArray<TracingRuntime::TypeErasedSamplePtrWithMutex>& GetTypeErasedSamplePtrs()
        const noexcept
    {
        return tracing_runtime_.type_erased_sample_ptrs_;
    }

  private:
    TracingRuntime& tracing_runtime_;
};

namespace
{

using ServiceInstanceElement = analysis::tracing::ServiceInstanceElement;
using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::WithArg;

const Configuration kEmptyConfiguration{Configuration::ServiceTypeDeployments{},
                                        Configuration::ServiceInstanceDeployments{},
                                        GlobalConfiguration{},
                                        TracingConfiguration{}};

const impl::tracing::ServiceElementInstanceIdentifierView kServiceElementInstanceIdentifier0{
    {"service_type_0",
     TracingRuntime::kDummyElementNameForShmRegisterCallback,
     TracingRuntime::kDummyElementTypeForShmRegisterCallback},
    "instance_specifier_0"};
const impl::tracing::ServiceElementInstanceIdentifierView kServiceElementInstanceIdentifier1{
    {"service_type_1",
     TracingRuntime::kDummyElementNameForShmRegisterCallback,
     TracingRuntime::kDummyElementTypeForShmRegisterCallback},
    "instance_specifier_1"};

const analysis::tracing::ShmObjectHandle kShmObjectHandle0{5U};
const analysis::tracing::ShmObjectHandle kShmObjectHandle1{6U};

void* const kStartAddress0{reinterpret_cast<void*>(0x10)};
void* const kStartAddress1{reinterpret_cast<void*>(0x20)};

const memory::shared::ISharedMemoryResource::FileDescriptor kShmFileDescriptor0{100U};
const memory::shared::ISharedMemoryResource::FileDescriptor kShmFileDescriptor1{200U};

constexpr std::size_t kNumberOfTracingServiceElements{5U};
constexpr std::uint8_t kFakeNumberOfIpcTracingSlotsPerServiceElement{7U};
constexpr auto kNumberOfTotalConfiguredTracingSlots{kNumberOfTracingServiceElements *
                                                    kFakeNumberOfIpcTracingSlotsPerServiceElement};

impl::tracing::TypeErasedSamplePtr CreateMockTypeErasedSamplePtr()
{
    mock_binding::SamplePtr<TestSampleType> mock_binding_sample_ptr = std::make_unique<TestSampleType>(42U);
    impl::SamplePtr<TestSampleType> dummy_sample_ptr{std::move(mock_binding_sample_ptr), SampleReferenceGuard{}};
    impl::tracing::TypeErasedSamplePtr dummy_type_erased_sample_ptr{std::move(dummy_sample_ptr)};
    return dummy_type_erased_sample_ptr;
}
impl::tracing::TypeErasedSamplePtr kDummyTypeErasedSamplePtr{CreateMockTypeErasedSamplePtr()};

class TracingRuntimeFixture : public ::testing::Test
{
  public:
    TracingRuntime tracing_runtime_{kNumberOfTotalConfiguredTracingSlots, kEmptyConfiguration};
};

using TracingRuntimeDataLossFlagFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeDataLossFlagFixture, DataLossFlagIsFalseByDefault)
{
    RecordProperty("Verifies", "SCR-18398046");
    RecordProperty("Description", "Checks that the transmission data loss flag is initially set to false.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a TracingRuntimeObject

    // Then getting the data loss flag before setting returns false
    EXPECT_FALSE(tracing_runtime_.GetDataLossFlag());
}

TEST_F(TracingRuntimeDataLossFlagFixture, GettingDataLossFlagAfterSettingReturnsTrue)
{
    RecordProperty("Verifies", "SCR-18398043");
    RecordProperty("Description", "Checks that getting the data loss flag will return the last value that was set.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a TracingRuntimeObject

    // When setting the data loss flag to true
    tracing_runtime_.SetDataLossFlag(true);

    // Then getting the data loss flag returns true
    EXPECT_TRUE(tracing_runtime_.GetDataLossFlag());
}

TEST_F(TracingRuntimeDataLossFlagFixture, GettingDataLossFlagAfterClearingReturnsFalse)
{
    // Given a TracingRuntimeObject

    // When setting the data loss flag to true
    tracing_runtime_.SetDataLossFlag(true);

    // and then setting the data loss flag to false
    tracing_runtime_.SetDataLossFlag(false);

    // Then getting the data loss flag returns false
    EXPECT_FALSE(tracing_runtime_.GetDataLossFlag());
}

using TracingRuntimeRegisterServiceElementFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeRegisterServiceElementFixture, SamplePtrArrayShouldBeNeverBeResized)
{
    // Given a TracingRuntimeObject
    const auto& sample_ptr_array = TracingRuntimeAttorney{tracing_runtime_}.GetTypeErasedSamplePtrs();

    // Then the sample ptr array should initially be of size kNumberOfTotalConfiguredTracingSlots
    const auto sample_ptr_array_size = sample_ptr_array.size();
    EXPECT_EQ(sample_ptr_array_size, kNumberOfTotalConfiguredTracingSlots);

    // When registering multiple sample ptrs
    for (TestSampleType i = 0; i < kNumberOfTracingServiceElements; ++i)
    {
        const auto service_element_tracing_data =
            tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);
        const auto service_element_range_start_for_equidistant_ranges =
            i * kFakeNumberOfIpcTracingSlotsPerServiceElement;
        EXPECT_EQ(service_element_tracing_data.service_element_range_start +
                      service_element_tracing_data.number_of_service_element_tracing_slots,
                  service_element_range_start_for_equidistant_ranges + kFakeNumberOfIpcTracingSlotsPerServiceElement);

        // Then the size of the array should never change
        const auto new_sample_ptr_array_size = sample_ptr_array.size();
        EXPECT_EQ(new_sample_ptr_array_size, kNumberOfTotalConfiguredTracingSlots);
    }
}

TEST_F(TracingRuntimeRegisterServiceElementFixture, RegisteringMultipleServiceElementsWillSetConsecutiveElementsInArray)
{
    // Given a TracingRuntimeObject
    constexpr std::uint8_t number_of_tracing_slots_0{7};
    constexpr std::uint8_t number_of_tracing_slots_1{3};

    // when registering 2 service elements with 7 and 3 tracing slots respectively
    const auto service_element_tracing_data_0 = tracing_runtime_.RegisterServiceElement(number_of_tracing_slots_0);
    const auto service_element_tracing_data_1 = tracing_runtime_.RegisterServiceElement(number_of_tracing_slots_1);

    // Then the first service elements range should start at 0 and second service elements range should start at 0 +
    // trace_slot_number_0
    EXPECT_EQ(service_element_tracing_data_0.service_element_range_start, 0);
    EXPECT_EQ(service_element_tracing_data_0.number_of_service_element_tracing_slots, number_of_tracing_slots_0);
    EXPECT_EQ(service_element_tracing_data_1.service_element_range_start, number_of_tracing_slots_0);
    EXPECT_EQ(service_element_tracing_data_1.number_of_service_element_tracing_slots, number_of_tracing_slots_1);
}

TEST(TracingRuntimeRegisterServiceElementDeathTest, RegisteringUInt16MaxServiceElementsWillTerminate)
{
    // Given a TracingRuntimeObject
    constexpr TracingRuntime::SamplePointerIndex number_of_configured_tracing_slots{
        std::numeric_limits<std::uint16_t>::max()};
    TracingRuntime tracing_runtime{number_of_configured_tracing_slots, kEmptyConfiguration};

    // When registering the maximum number of allowed service elements
    for (std::size_t i = 0; i < number_of_configured_tracing_slots; ++i)
    {
        score::cpp::ignore = tracing_runtime.RegisterServiceElement(1U);
    }
    // Then we don't crash
    // But then when we register another service element we terminate
    EXPECT_DEATH(score::cpp::ignore = tracing_runtime.RegisterServiceElement(1U), ".*");
}

using TracingRuntimeRegisterServiceElementFixtureDeathTest = TracingRuntimeRegisterServiceElementFixture;
TEST_F(TracingRuntimeRegisterServiceElementFixtureDeathTest, RegisteringTooManyServiceElementsWillTerminate)
{
    // Given a TracingRuntimeObject

    // When registering the maximum number of allowed service elements
    for (std::size_t i = 0; i < kNumberOfTracingServiceElements; ++i)
    {
        score::cpp::ignore = tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);
    }
    // Then we don't crash
    // But then when we register another service element we terminate
    EXPECT_DEATH(score::cpp::ignore = tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement),
                 ".*");
}

TEST_F(TracingRuntimeRegisterServiceElementFixtureDeathTest, RegisteringServiceElementWithZeroTracingSlotsWillTerminate)
{
    // Given a TracingRuntimeObject

    // When registering the maximum number of allowed service elements
    // Then we don't crash
    const TracingRuntime::TracingSlotSizeType number_of_ipc_tracing_slots{0U};
    EXPECT_DEATH(score::cpp::ignore = tracing_runtime_.RegisterServiceElement(number_of_ipc_tracing_slots), ".*");
}

using TracingRuntimeTypeErasedSamplePtrFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeTypeErasedSamplePtrFixture,
       EmplacingTypeErasedSamplePtrOnceWillReturnFirstIndexOfTypeErasedSamplePtrs)
{
    // Given a TracingRuntimeObject with a registered service element
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);

    // When emplacing a type erased sample ptr
    const auto trace_context_id_result =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data);

    // Then the TraceContextId should equal the first index of the type_erased_sample_ptrs_ array
    ASSERT_TRUE(trace_context_id_result.has_value());
    EXPECT_EQ(trace_context_id_result.value(), 0U);
}

TEST_F(TracingRuntimeTypeErasedSamplePtrFixture,
       EmplacingTypeErasedSamplePtrForTheSameServiceElementWillReturnNextIndexOfTypeErasedSamplePtrs)
{
    // Given a TracingRuntimeObject with a registered service element and an emplaced type erased sample ptr
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);
    score::cpp::ignore =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data);

    // When emplacing another type erased sample ptr for the same service element
    const auto trace_context_id_result =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data);

    // Then the TraceContextId should equal the second index of the type_erased_sample_ptrs_ array
    ASSERT_TRUE(trace_context_id_result.has_value());
    EXPECT_EQ(trace_context_id_result.value(), 1U);
}

TEST_F(TracingRuntimeTypeErasedSamplePtrFixture,
       EmplacingTypeErasedSamplePtrForDifferentSameServiceElementWillReturnValidId)
{
    // Given a TracingRuntimeObject with 2 registered service elements and an emplaced type erased sample ptr for the
    // first service element
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);
    const auto service_element_tracing_data2 =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);
    score::cpp::ignore =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data);

    // When emplacing a type erased sample ptr for the second service element
    const auto trace_context_id_result = tracing_runtime_.EmplaceTypeErasedSamplePtr(
        std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data2);

    // Then the TraceContextId should be the first slot after the first service elements slots
    ASSERT_TRUE(trace_context_id_result.has_value());
    EXPECT_EQ(trace_context_id_result.value(), kFakeNumberOfIpcTracingSlotsPerServiceElement);
}

TEST_F(TracingRuntimeTypeErasedSamplePtrFixture,
       EmplacingMaxNumberOfTypeErasedSamplePtrsReturnsSequentialTraceContextIds)
{
    // Given a TracingRuntimeObject with a registered service element
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);

    // When emplacing the max number of type erased sample ptrs it's allowed
    for (std::size_t i = 0; i < kFakeNumberOfIpcTracingSlotsPerServiceElement; ++i)
    {
        const auto trace_context_id_result = tracing_runtime_.EmplaceTypeErasedSamplePtr(
            std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data);

        // Then each TraceContextId will be sequential
        ASSERT_TRUE(trace_context_id_result.has_value());
        EXPECT_EQ(trace_context_id_result.value(), i);
    }
}

TEST_F(TracingRuntimeTypeErasedSamplePtrFixture,
       EmplacingTypeErasedSamplePtrWhenNoTypeErasedSamplePtrSlotsAvailableForServiceElementReturnsEmptyOptional)
{
    // Given a TracingRuntimeObject with a registered service element which has emplaced the max number of type erased
    // sample ptrs it's allowed
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);

    for (std::size_t i = 0; i < kFakeNumberOfIpcTracingSlotsPerServiceElement; ++i)
    {
        score::cpp::ignore = tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr),
                                                                  service_element_tracing_data);
    }

    // When emplacing another type erased sample ptr
    const auto trace_context_id_result =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data);

    // Then the an empty optional should be returned
    ASSERT_FALSE(trace_context_id_result.has_value());
}

TEST_F(TracingRuntimeTypeErasedSamplePtrFixture, EmplacingTypeErasedSamplePtrDoesNotDestroySamplePtrUntilItIsCleared)
{
    class DestructorTracer
    {
      public:
        DestructorTracer(bool& was_destructed) : is_owner_{true}, was_destructed_{was_destructed} {}

        ~DestructorTracer()
        {
            if (is_owner_)
            {
                was_destructed_ = true;
            }
        }

        DestructorTracer(const DestructorTracer&) = delete;
        DestructorTracer& operator=(const DestructorTracer&) = delete;
        DestructorTracer& operator=(DestructorTracer&&) = delete;

        DestructorTracer(DestructorTracer&& other) : is_owner_{true}, was_destructed_{other.was_destructed_}
        {
            other.is_owner_ = false;
        }

      private:
        bool is_owner_;
        bool& was_destructed_;
    };

    bool was_destructed{false};
    mock_binding::SamplePtr<DestructorTracer> pointer = std::make_unique<DestructorTracer>(was_destructed);
    impl::SamplePtr<DestructorTracer> sample_ptr{std::move(pointer), SampleReferenceGuard{}};
    impl::tracing::TypeErasedSamplePtr type_erased_sample_ptr{std::move(sample_ptr)};

    // Given a TracingRuntimeObject

    // When registering a service element
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);

    // and setting the type erased sample ptr which will return a trace_context_id with value
    const auto trace_context_id =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(type_erased_sample_ptr), service_element_tracing_data);

    const auto trace_context_id_val = trace_context_id.value();

    // and the sample ptr will not be destroyed
    EXPECT_FALSE(was_destructed);

    // Until after the type erased sample ptr is cleared
    tracing_runtime_.ClearTypeErasedSamplePtr(trace_context_id_val);
    EXPECT_TRUE(was_destructed);
}

TEST_F(TracingRuntimeTypeErasedSamplePtrFixture, ServiceElementTracingIsActiveAfterEmplacingTypeErasedSamplePtr)
{
    RecordProperty("Verifies", "SCR-18390315");
    RecordProperty(
        "Description",
        "Calling EmplaceTypeErasedSamplePtr will store that data in shared memory is currently being traced.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a TracingRuntimeObject

    // When registering a service element with 7 tracing slots
    const auto service_element_tracing_data = tracing_runtime_.RegisterServiceElement(7U);

    // Then the service element is initially inactive
    EXPECT_FALSE(
        tracing_runtime_.IsTracingSlotUsed(static_cast<impl::tracing::ITracingRuntimeBinding::TraceContextId>(0U)));

    // and when setting the type erased sample ptr
    const auto completed_service_context_id =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data);
    // Then the service element is active
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(completed_service_context_id.value()));
}

TEST_F(TracingRuntimeTypeErasedSamplePtrFixture, ClearTypeErasedSamplePtrSetsSlotForTraceContextIdAsUnused)
{
    // Given a TracingRuntimeObject with a registered service element
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);

    // and given that 2 type erased sample ptr have been successfully emplaced
    impl::tracing::TypeErasedSamplePtr dummy_type_erased_sample_ptr_2{CreateMockTypeErasedSamplePtr()};
    const auto trace_context_id_1 =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data)
            .value();
    const auto trace_context_id_2 =
        tracing_runtime_
            .EmplaceTypeErasedSamplePtr(std::move(dummy_type_erased_sample_ptr_2), service_element_tracing_data)
            .value();
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_1));
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_2));

    // When clearing the type erased sample ptr corresponding to the first TraceContextId
    tracing_runtime_.ClearTypeErasedSamplePtr(trace_context_id_1);

    // Then the slot corresponding to that TraceContextId is unused
    EXPECT_FALSE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_1));
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_2));
}

TEST_F(TracingRuntimeTypeErasedSamplePtrFixture,
       AllServiceElementTracingSlotsAreUnusedAfterClearingTypeErasedSamplePtrs)
{
    // Given a TracingRuntimeObject with a registered service element
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);

    // and given that 2 type erased sample ptr have been successfully emplaced
    impl::tracing::TypeErasedSamplePtr dummy_type_erased_sample_ptr_2{CreateMockTypeErasedSamplePtr()};
    const auto trace_context_id_1 =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data)
            .value();
    const auto trace_context_id_2 =
        tracing_runtime_
            .EmplaceTypeErasedSamplePtr(std::move(dummy_type_erased_sample_ptr_2), service_element_tracing_data)
            .value();
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_1));
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_2));

    // When clearing all type erased sample ptr corresponding to the ServiceElementTracingData
    tracing_runtime_.ClearTypeErasedSamplePtrs(service_element_tracing_data);

    // Then all slots corresponding to that ServiceElementTracingData are unused
    EXPECT_FALSE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_1));
    EXPECT_FALSE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_2));
}

using TracingRuntimeTypeErasedSamplePtrDeathTest = TracingRuntimeTypeErasedSamplePtrFixture;
TEST_F(TracingRuntimeTypeErasedSamplePtrDeathTest,
       EmplacingTypeErasedSamplePtrBeforeRegisteringServiceElementTerminates)
{
    impl::tracing::ServiceElementTracingData invalid_service_element_tracing_data{1U, 1U};

    // Given a TracingRuntimeObject

    // When a service element has not yet been registered

    // Then setting the type erased sample ptr will terminate
    EXPECT_DEATH(score::cpp::ignore = tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr),
                                                                           invalid_service_element_tracing_data),
                 ".*");
}

using TracingRuntimeUnregisterShmObjectFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeUnregisterShmObjectFixture, UnregisteringShmObjectWhichWasNeverRegisteredDoesNotTerminate)
{
    // Given a TracingRuntimeObject

    // When unregistering a shm object that was never registered
    // Then we don't terminate
    tracing_runtime_.UnregisterShmObject(kServiceElementInstanceIdentifier0);
}

using TracingRuntimeRegisterShmObjectDeathTest = TracingRuntimeFixture;
TEST_F(TracingRuntimeRegisterShmObjectDeathTest, RegisterShmObjectWithNotTheExpectedDummyElementNameTerminates)
{
    // Given a TracingRuntimeObject

    // and a ServiceElementInstanceIdentifier with a wrong/unexpected element name
    const impl::tracing::ServiceElementInstanceIdentifierView kServiceElementInstanceIdentifierInvalidName{
        {"service_type_0", "some_name", TracingRuntime::kDummyElementTypeForShmRegisterCallback},
        "instance_specifier_0"};

    // Using the invalid service element instance identifier leads to termination.
    EXPECT_DEATH(tracing_runtime_.RegisterShmObject(
                     kServiceElementInstanceIdentifierInvalidName, kShmObjectHandle0, kStartAddress0),
                 ".*");
}

TEST_F(TracingRuntimeRegisterShmObjectDeathTest, RegisterShmObjectWithNotTheExpectedDummyElementTypeTerminates)
{
    // Given a TracingRuntimeObject

    // and a ServiceElementInstanceIdentifier with a wrong/unexpected element type
    const impl::tracing::ServiceElementInstanceIdentifierView kServiceElementInstanceIdentifierInvalidType{
        {"service_type_0", TracingRuntime::kDummyElementNameForShmRegisterCallback, impl::ServiceElementType::FIELD},
        "instance_specifier_0"};

    // Using the invalid service element instance identifier leads to termination.
    EXPECT_DEATH(tracing_runtime_.RegisterShmObject(
                     kServiceElementInstanceIdentifierInvalidType, kShmObjectHandle0, kStartAddress0),
                 ".*");
}

using TracingRuntimeShmObjectHandleFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeShmObjectHandleFixture, GettingShmObjectHandleAndStartAddressAfterRegisteringReturnsHandle)
{
    // Given a TracingRuntimeObject

    // When registering 2 object handles and start addresses
    tracing_runtime_.RegisterShmObject(kServiceElementInstanceIdentifier0, kShmObjectHandle0, kStartAddress0);
    tracing_runtime_.RegisterShmObject(kServiceElementInstanceIdentifier1, kShmObjectHandle1, kStartAddress1);

    // Then getting the shm object handles returns the correct handles
    const auto returned_handle_0 = tracing_runtime_.GetShmObjectHandle(kServiceElementInstanceIdentifier0);
    ASSERT_TRUE(returned_handle_0.has_value());
    EXPECT_EQ(returned_handle_0.value(), kShmObjectHandle0);

    const auto returned_handle_1 = tracing_runtime_.GetShmObjectHandle(kServiceElementInstanceIdentifier1);
    ASSERT_TRUE(returned_handle_1.has_value());
    EXPECT_EQ(returned_handle_1.value(), kShmObjectHandle1);

    // And getting the start addresses returns the correct addresses
    const auto returned_start_address_0 = tracing_runtime_.GetShmRegionStartAddress(kServiceElementInstanceIdentifier0);
    ASSERT_TRUE(returned_start_address_0.has_value());
    EXPECT_EQ(returned_start_address_0.value(), kStartAddress0);

    const auto returned_start_address_1 = tracing_runtime_.GetShmRegionStartAddress(kServiceElementInstanceIdentifier1);
    ASSERT_TRUE(returned_start_address_1.has_value());
    EXPECT_EQ(returned_start_address_1.value(), kStartAddress1);
}

TEST_F(TracingRuntimeShmObjectHandleFixture, GettingShmObjectHandleAndStartAddressWithoutRegistrationReturnsEmpty)
{
    // Given a TracingRuntimeObject

    // When getting shm object handles before registering them
    const auto returned_handle_0 = tracing_runtime_.GetShmObjectHandle(kServiceElementInstanceIdentifier0);
    const auto returned_handle_1 = tracing_runtime_.GetShmObjectHandle(kServiceElementInstanceIdentifier1);

    // Then the results are empty optionals
    EXPECT_FALSE(returned_handle_0.has_value());
    EXPECT_FALSE(returned_handle_1.has_value());

    // When getting start addresses before registering them
    const auto returned_start_address_0 = tracing_runtime_.GetShmRegionStartAddress(kServiceElementInstanceIdentifier0);
    const auto returned_start_address_1 = tracing_runtime_.GetShmRegionStartAddress(kServiceElementInstanceIdentifier1);

    // Then the results are empty optionals
    EXPECT_FALSE(returned_start_address_0);
    EXPECT_FALSE(returned_start_address_1);
}

TEST_F(TracingRuntimeShmObjectHandleFixture, GettingShmObjectHandleAndStartAddressAfterUnregisteringReturnsEmpty)
{
    // Given a TracingRuntimeObject

    // When registering 2 object handles and start addresses
    tracing_runtime_.RegisterShmObject(kServiceElementInstanceIdentifier0, kShmObjectHandle0, kStartAddress0);
    tracing_runtime_.RegisterShmObject(kServiceElementInstanceIdentifier1, kShmObjectHandle1, kStartAddress1);

    // And then unregistering the first
    tracing_runtime_.UnregisterShmObject(kServiceElementInstanceIdentifier0);

    // Then getting the shm object handles returns the correct handles only for the second
    const auto returned_handle_0 = tracing_runtime_.GetShmObjectHandle(kServiceElementInstanceIdentifier0);
    EXPECT_FALSE(returned_handle_0.has_value());

    const auto returned_handle_1 = tracing_runtime_.GetShmObjectHandle(kServiceElementInstanceIdentifier1);
    ASSERT_TRUE(returned_handle_1.has_value());
    EXPECT_EQ(returned_handle_1.value(), kShmObjectHandle1);

    // And getting the start addresses returns the correct addresses only for the second
    const auto returned_start_address_0 = tracing_runtime_.GetShmRegionStartAddress(kServiceElementInstanceIdentifier0);
    EXPECT_FALSE(returned_start_address_0.has_value());

    const auto returned_start_address_1 = tracing_runtime_.GetShmRegionStartAddress(kServiceElementInstanceIdentifier1);
    ASSERT_TRUE(returned_start_address_1.has_value());
    EXPECT_EQ(returned_start_address_1.value(), kStartAddress1);
}

TEST_F(TracingRuntimeShmObjectHandleFixture,
       GettingShmObjectHandleAfterRegisteringAndUnregisteringShmObjectReturnsEmpty)
{
    // Given a TracingRuntimeObject with a shm object which was registered and then unregistered
    tracing_runtime_.RegisterShmObject(kServiceElementInstanceIdentifier0, kShmObjectHandle0, kStartAddress0);
    tracing_runtime_.UnregisterShmObject(kServiceElementInstanceIdentifier0);

    // When getting the shm object handle
    const auto returned_handle = tracing_runtime_.GetShmObjectHandle(kServiceElementInstanceIdentifier0);

    // Then an empty result will be returned
    EXPECT_FALSE(returned_handle.has_value());
}

TEST_F(TracingRuntimeShmObjectHandleFixture, GettingShmObjectHandleAfterReRegisteringShmObjectReturnsHandle)
{
    // Given a TracingRuntimeObject with a shm object which was registered, unregistered and then registered again
    tracing_runtime_.RegisterShmObject(kServiceElementInstanceIdentifier0, kShmObjectHandle0, kStartAddress0);
    tracing_runtime_.UnregisterShmObject(kServiceElementInstanceIdentifier0);
    tracing_runtime_.RegisterShmObject(kServiceElementInstanceIdentifier0, kShmObjectHandle0, kStartAddress0);

    // When getting the shm object handle
    const auto returned_handle = tracing_runtime_.GetShmObjectHandle(kServiceElementInstanceIdentifier0);

    // Then the correct handle is returned
    EXPECT_TRUE(returned_handle.has_value());
    EXPECT_EQ(returned_handle.value(), kShmObjectHandle0);
}

using TracingRuntimeShmObjectHandleDeathTest = TracingRuntimeFixture;
TEST_F(TracingRuntimeShmObjectHandleDeathTest, CallingRegisterShmObjectTwiceForTheSameServiceElementTerminates)
{
    // Given a TracingRuntimeObject which has already registered a shm object for a service element
    tracing_runtime_.RegisterShmObject(kServiceElementInstanceIdentifier0, kShmObjectHandle0, kStartAddress0);

    // When calling RegisterShmObject again for the same service element
    // Then the program terminates
    EXPECT_DEATH(
        tracing_runtime_.RegisterShmObject(kServiceElementInstanceIdentifier0, kShmObjectHandle0, kStartAddress0),
        ".*");
}

using TracingRuntimeCachedFileDescriptorFixture = TracingRuntimeFixture;
TEST_F(TracingRuntimeCachedFileDescriptorFixture, GettingFileDescriptorAfterCachingReturnsFileDescriptor)
{
    // Given a TracingRuntimeObject

    // When caching 2 file descriptors
    tracing_runtime_.CacheFileDescriptorForReregisteringShmObject(
        kServiceElementInstanceIdentifier0, kShmFileDescriptor0, kStartAddress0);
    tracing_runtime_.CacheFileDescriptorForReregisteringShmObject(
        kServiceElementInstanceIdentifier1, kShmFileDescriptor1, kStartAddress1);

    // Then getting the file descriptors returns the correct descriptors
    const auto returned_file_descriptor_0 =
        tracing_runtime_.GetCachedFileDescriptorForReregisteringShmObject(kServiceElementInstanceIdentifier0);
    ASSERT_TRUE(returned_file_descriptor_0.has_value());
    EXPECT_EQ(returned_file_descriptor_0.value().first, kShmFileDescriptor0);
    EXPECT_EQ(returned_file_descriptor_0.value().second, kStartAddress0);

    const auto returned_file_descriptor_1 =
        tracing_runtime_.GetCachedFileDescriptorForReregisteringShmObject(kServiceElementInstanceIdentifier1);
    ASSERT_TRUE(returned_file_descriptor_1.has_value());
    EXPECT_EQ(returned_file_descriptor_1.value().first, kShmFileDescriptor1);
    EXPECT_EQ(returned_file_descriptor_1.value().second, kStartAddress1);
}

TEST_F(TracingRuntimeCachedFileDescriptorFixture, GettingFileDescriptorWithoutCachingReturnsEmpty)
{
    // Given a TracingRuntimeObject

    // When getting the file descriptors before caching them
    const auto returned_file_descriptor_0 =
        tracing_runtime_.GetCachedFileDescriptorForReregisteringShmObject(kServiceElementInstanceIdentifier0);
    const auto returned_file_descriptor_1 =
        tracing_runtime_.GetCachedFileDescriptorForReregisteringShmObject(kServiceElementInstanceIdentifier1);

    // Then the results are empty optionals
    EXPECT_FALSE(returned_file_descriptor_0.has_value());
    EXPECT_FALSE(returned_file_descriptor_1.has_value());
}

TEST_F(TracingRuntimeCachedFileDescriptorFixture, GettingFileDescriptorAfterClearingReturnsEmpty)
{
    // Given a TracingRuntimeObject

    // When caching 2 file descriptors
    tracing_runtime_.CacheFileDescriptorForReregisteringShmObject(
        kServiceElementInstanceIdentifier0, kShmFileDescriptor0, kStartAddress0);
    tracing_runtime_.CacheFileDescriptorForReregisteringShmObject(
        kServiceElementInstanceIdentifier1, kShmFileDescriptor1, kStartAddress1);

    // And then clearing the first
    tracing_runtime_.ClearCachedFileDescriptorForReregisteringShmObject(kServiceElementInstanceIdentifier0);

    // Then getting the file descriptors returns the correct file descriptor only for the second
    const auto returned_file_descriptor_0 =
        tracing_runtime_.GetCachedFileDescriptorForReregisteringShmObject(kServiceElementInstanceIdentifier0);
    EXPECT_FALSE(returned_file_descriptor_0.has_value());

    const auto returned_file_descriptor_1 =
        tracing_runtime_.GetCachedFileDescriptorForReregisteringShmObject(kServiceElementInstanceIdentifier1);
    ASSERT_TRUE(returned_file_descriptor_1.has_value());
    EXPECT_EQ(returned_file_descriptor_1.value().first, kShmFileDescriptor1);
    EXPECT_EQ(returned_file_descriptor_1.value().second, kStartAddress1);
}

TEST_F(TracingRuntimeCachedFileDescriptorFixture, ClearingFileDescriptorThatWasNeverCachedDoesNotTerminate)
{
    // Given a TracingRuntimeObject

    // When clearing a filedescriptor which was never cached
    tracing_runtime_.ClearCachedFileDescriptorForReregisteringShmObject(kServiceElementInstanceIdentifier0);

    // Then we don't crash
}

using TracingRuntimeCachedFileDescriptorDeathTest = TracingRuntimeCachedFileDescriptorFixture;
TEST_F(TracingRuntimeCachedFileDescriptorDeathTest, CachingTheSameFileDescriptorTwiceTerminates)
{
    // Given a TracingRuntimeObject which has cached a file descriptor
    tracing_runtime_.CacheFileDescriptorForReregisteringShmObject(
        kServiceElementInstanceIdentifier0, kShmFileDescriptor0, kStartAddress0);

    // When caching the same file descriptor again
    // Then we terminate
    EXPECT_DEATH(tracing_runtime_.CacheFileDescriptorForReregisteringShmObject(
                     kServiceElementInstanceIdentifier0, kShmFileDescriptor1, kStartAddress1),
                 ".*");
}

class RegisterWithGenericTraceApiFixture : public testing::Test
{
  public:
    RegisterWithGenericTraceApiFixture()
        : trace_client_id_{42},
          generic_trace_api_mock_{std::make_unique<score::analysis::tracing::TraceLibraryMock>()},
          dummy_configuration_{GenerateDummyConfiguration()},
          tracing_runtime_{kNumberOfTotalConfiguredTracingSlots, *dummy_configuration_}
    {
    }

    std::unique_ptr<Configuration> GenerateDummyConfiguration()
    {
        TracingConfiguration dummy_tracing_configuration{};
        dummy_tracing_configuration.SetApplicationInstanceID(application_instance_id_);
        return std::make_unique<Configuration>(Configuration::ServiceTypeDeployments{},
                                               Configuration::ServiceInstanceDeployments{},
                                               GlobalConfiguration{},
                                               std::move(dummy_tracing_configuration));
    }

  protected:
    static constexpr char application_instance_id_[] = "MyApp";

    const std::uint8_t trace_client_id_;
    std::unique_ptr<score::analysis::tracing::TraceLibraryMock> generic_trace_api_mock_;
    std::unique_ptr<Configuration> dummy_configuration_;
    TracingRuntime tracing_runtime_;
};

TEST_F(RegisterWithGenericTraceApiFixture, Registration_OK)
{
    // Given a TracingRuntimeObject

    // expect, that it calls RegisterClient with the configured application_instance_id_ at the GenericTraceAPI
    EXPECT_CALL(*generic_trace_api_mock_.get(),
                RegisterClient(analysis::tracing::BindingType::kLoLa, application_instance_id_))
        .WillOnce(Return(trace_client_id_));
    // and expect, that it calls then RegisterTraceDoneCB at the GenericTraceAPI with the trace client id, which has
    // been returned by the RegisterClient call
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterTraceDoneCB(trace_client_id_, _))
        .WillOnce(Return(score::Result<Blank>{}));

    // expect the UuT to return true, when we call RegisterWithGenericTraceApi on it
    EXPECT_TRUE(tracing_runtime_.RegisterWithGenericTraceApi());
}

TEST_F(RegisterWithGenericTraceApiFixture, Registration_Error)
{
    // Given a TracingRuntimeObject

    // expect, that it calls RegisterClient with the configured application_instance_id_ at the GenericTraceAPI
    EXPECT_CALL(*generic_trace_api_mock_.get(),
                RegisterClient(analysis::tracing::BindingType::kLoLa, application_instance_id_))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kNotEnoughMemoryRecoverable)));

    // expect the UuT to return false, when we call RegisterWithGenericTraceApi on it
    EXPECT_FALSE(tracing_runtime_.RegisterWithGenericTraceApi());
}

TEST_F(RegisterWithGenericTraceApiFixture, RegistrationTraceDoneCB_Error)
{
    // Given a TracingRuntimeObject

    // expect, that it calls RegisterClient with the configured application_instance_id_ at the GenericTraceAPI
    EXPECT_CALL(*generic_trace_api_mock_.get(),
                RegisterClient(analysis::tracing::BindingType::kLoLa, application_instance_id_))
        .WillOnce(Return(trace_client_id_));
    // and expect, that it calls then RegisterTraceDoneCB at the GenericTraceAPI with the trace client id, which has
    // been returned by the RegisterClient call, but it returns a failure
    EXPECT_CALL(*generic_trace_api_mock_.get(), RegisterTraceDoneCB(trace_client_id_, _))
        .WillOnce(Return(score::MakeUnexpected(analysis::tracing::ErrorCode::kDaemonConnectionFailedFatal)));

    // expect the UuT to return false, when we call RegisterWithGenericTraceApi on it
    EXPECT_FALSE(tracing_runtime_.RegisterWithGenericTraceApi());
}

class TraceDoneCallbackFixture : public RegisterWithGenericTraceApiFixture
{
  public:
    TraceDoneCallbackFixture()
    {
        // expect, that it calls RegisterClient with the configured application_instance_id_ at the GenericTraceAPI
        ON_CALL(*generic_trace_api_mock_.get(),
                RegisterClient(analysis::tracing::BindingType::kLoLa, application_instance_id_))
            .WillByDefault(Return(trace_client_id_));

        // and expect, that it calls then RegisterTraceDoneCB at the GenericTraceAPI with the trace client id, which has
        // been returned by the RegisterClient call
        ON_CALL(*generic_trace_api_mock_.get(), RegisterTraceDoneCB(trace_client_id_, _))
            .WillByDefault(WithArg<1>(
                Invoke([this](auto trace_done_callback) -> analysis::tracing::RegisterTraceDoneCallBackResult {
                    trace_done_callback_ = std::move(trace_done_callback);
                    static_cast<void>(trace_done_callback);
                    return score::Result<Blank>{};
                })));
    }

    analysis::tracing::TraceDoneCallBackType trace_done_callback_;
};

TEST_F(TraceDoneCallbackFixture, ServiceElementTracingIsInactiveAfterCallingTraceDoneCallbackWithCorrectContextId)
{
    RecordProperty("Verifies", "SCR-18391091, SCR-18385218");
    RecordProperty("Description",
                   "Calling the trace done callback with the correct context id will clear that data in shared memory "
                   "is currently being traced.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    mock_binding::SamplePtr<TestSampleType> mock_binding_sample_pointer_1 = std::make_unique<TestSampleType>(24U);
    impl::SamplePtr<TestSampleType> dummy_sample_pointer_1{std::move(mock_binding_sample_pointer_1),
                                                           SampleReferenceGuard{}};
    impl::tracing::TypeErasedSamplePtr dummy_type_erased_sample_pointer_1{std::move(dummy_sample_pointer_1)};

    // Given a TracingRuntimeObject

    // expect the UuT to return true, when we call RegisterWithGenericTraceApi on it
    ASSERT_TRUE(tracing_runtime_.RegisterWithGenericTraceApi());

    // When registering a service element
    const auto service_element_tracing_data_0 =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);
    const auto service_element_tracing_data_1 =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);

    // and when setting the type erased sample ptr with the provided TraceContextIds
    const auto trace_context_id_0 = tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr),
                                                                                service_element_tracing_data_0);
    const auto trace_context_id_1 = tracing_runtime_.EmplaceTypeErasedSamplePtr(
        std::move(dummy_type_erased_sample_pointer_1), service_element_tracing_data_1);

    // Then trace context ids should have value
    ASSERT_TRUE(trace_context_id_0.has_value());
    ASSERT_TRUE(trace_context_id_1.has_value());
    const auto trace_context_id_0_val = trace_context_id_0.value();
    const auto trace_context_id_1_val = trace_context_id_1.value();

    // and tracing should be marked as active for both TraceContextIds
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_0_val));
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_1_val));

    // Then when calling the trace done callback with the second TraceContextId
    trace_done_callback_(trace_context_id_1_val);

    // Then tracing should no longer be active for the second TraceContextId
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_0_val));
    EXPECT_FALSE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_1_val));

    // and when calling the trace done callback with the first TraceContextId
    trace_done_callback_(trace_context_id_0_val);

    // then tracing should no longer be active for either TraceContextId
    EXPECT_FALSE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_0_val));
    EXPECT_FALSE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_1_val));
}

TEST_F(TraceDoneCallbackFixture,
       ServiceElementTracingIsUnchangedAfterCallingTraceDoneCallbackWithCorrectContextIdASecondTime)
{
    RecordProperty("Verifies", "SCR-18391091");
    RecordProperty("Description",
                   "Calling the trace done callback with the correct context id a second time will print a warning.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a TracingRuntimeObject

    // expect the UuT to return true, when we call RegisterWithGenericTraceApi on it
    ASSERT_TRUE(tracing_runtime_.RegisterWithGenericTraceApi());

    // When registering a service element
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);

    // and when setting the type erased sample ptr with the provided TraceContextId
    const auto trace_context_id =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data);

    // Then trace_context_id has a value
    EXPECT_TRUE(trace_context_id.has_value());
    const auto trace_context_id_val = trace_context_id.value();
    // Then tracing should be marked as active
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_val));

    // Then when calling the trace done callback with the provided TraceContextId
    trace_done_callback_(trace_context_id_val);

    // and tracing should no longer be active
    EXPECT_FALSE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_val));

    // capture stdout output during trace done callback call.
    testing::internal::CaptureStdout();

    // Then when calling the trace done callback with the provided TraceContextId again
    trace_done_callback_(trace_context_id_val);

    // stop capture and get captured data.
    std::string log_output = testing::internal::GetCapturedStdout();
    const char log_warn_snippet[] = "log warn";

    std::stringstream text_snippet{};
    text_snippet << "Lola TracingRuntime: TraceDoneCB with TraceContextId " << trace_context_id_val
                 << " was not pending but has been called anyway. This is expected to occur if the trace done "
                    "callback is called after an event/field has been stop offered. Ignoring callback.";

    // Then a warning message should be logged (mw::log)
    auto first_offset = log_output.find(log_warn_snippet);
    EXPECT_TRUE(first_offset != log_output.npos);
    EXPECT_TRUE(log_output.find(text_snippet.str(), first_offset) != log_output.npos);
}

TEST_F(TraceDoneCallbackFixture, ServiceElementTracingIsUnchangedAfterCallingTraceDoneCallbackWithIncorrectContextId)
{
    RecordProperty("Verifies", "SCR-18391091");
    RecordProperty("Description", "Calling the trace done callback with an incorrect context id will print a warning.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a TracingRuntimeObject

    // expect the UuT to return true, when we call RegisterWithGenericTraceApi on it
    ASSERT_TRUE(tracing_runtime_.RegisterWithGenericTraceApi());

    // When registering a service element
    const auto service_element_tracing_data =
        tracing_runtime_.RegisterServiceElement(kFakeNumberOfIpcTracingSlotsPerServiceElement);

    // and when setting the type erased sample ptr with the provided service_element_tracing_data
    const auto trace_context_id =
        tracing_runtime_.EmplaceTypeErasedSamplePtr(std::move(kDummyTypeErasedSamplePtr), service_element_tracing_data);

    // Then the returned trace context_id should have a value
    EXPECT_TRUE(trace_context_id.has_value());
    const auto trace_context_id_val = trace_context_id.value();
    // and tracing should be marked as active
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_val));

    // capture stdout output during trace done callback call.
    testing::internal::CaptureStdout();

    // Then when calling the trace done callback with a different TraceContextId
    analysis::tracing::TraceContextId different_trace_context_id = trace_context_id_val + 1U;
    trace_done_callback_(different_trace_context_id);

    // stop capture and get captured data.
    std::string log_output = testing::internal::GetCapturedStdout();
    const char log_warn_snippet[] = "log warn";

    std::stringstream text_snippet{};
    text_snippet << "Lola TracingRuntime: TraceDoneCB with TraceContextId " << different_trace_context_id
                 << " was not pending but has been called anyway. This is expected to occur if the trace done "
                    "callback is called after an event/field has been stop offered. Ignoring callback.";

    // Then a warning message should be logged (mw::log)
    auto first_offset = log_output.find(log_warn_snippet);
    EXPECT_TRUE(first_offset != log_output.npos);
    EXPECT_TRUE(log_output.find(text_snippet.str(), first_offset) != log_output.npos);

    // and tracing should still be active
    EXPECT_TRUE(tracing_runtime_.IsTracingSlotUsed(trace_context_id_val));
}

class TracingRuntimeConvertToTracingServiceInstanceElementFixture : public testing::Test
{
  public:
    const std::string service_type_name_{"my_service_type"};
    const std::uint32_t major_version_number_{20U};
    const std::uint32_t minor_version_number_{30U};

    const InstanceSpecifier instance_specifier_{InstanceSpecifier::Create("my_instance_specifier").value()};

    const LolaServiceInstanceId::InstanceId instance_id_{12U};
    const LolaServiceId service_id_{13U};
    const std::string event_name_{"my_event_name"};
    const LolaEventId event_id_{2U};
    const std::string field_name_{"my_field_name"};
    const LolaFieldId field_id_{3U};
};

TEST_F(TracingRuntimeConvertToTracingServiceInstanceElementFixture,
       ConvertFunctionGeneratesServiceInstanceElementFromConfig)
{
    const auto service_identifier_type =
        make_ServiceIdentifierType(service_type_name_, major_version_number_, minor_version_number_);
    const LolaServiceInstanceDeployment lola_service_instance_deployment{LolaServiceInstanceId{instance_id_}};
    const auto lola_service_type_deployment =
        CreateTypeDeployment(service_id_, {{event_name_, event_id_}}, {{field_name_, field_id_}});

    Configuration configuration{
        {{service_identifier_type, ServiceTypeDeployment{lola_service_type_deployment}}},
        {{instance_specifier_,
          ServiceInstanceDeployment{
              service_identifier_type, lola_service_instance_deployment, QualityType::kInvalid, instance_specifier_}}},
        GlobalConfiguration{},
        TracingConfiguration{}};

    const ServiceInstanceElement expected_service_instance_element_event{
        static_cast<ServiceInstanceElement::ServiceIdType>(service_id_),
        major_version_number_,
        minor_version_number_,
        static_cast<ServiceInstanceElement::InstanceIdType>(instance_id_),
        static_cast<ServiceInstanceElement::EventIdType>(event_id_)};
    const ServiceInstanceElement expected_service_instance_element_field{
        static_cast<ServiceInstanceElement::ServiceIdType>(service_id_),
        major_version_number_,
        minor_version_number_,
        static_cast<ServiceInstanceElement::InstanceIdType>(instance_id_),
        static_cast<ServiceInstanceElement::FieldIdType>(field_id_)};

    // Given a TracingRuntimeObject with a provided configuration object
    TracingRuntime tracing_runtime{kNumberOfTotalConfiguredTracingSlots, configuration};

    // When converting ServiceElementIdentifierViews to ServiceInstanceElements
    const auto instance_specifier_std_string_view = instance_specifier_.ToString();
    const auto instance_specifier_string_view =
        score::cpp::string_view{instance_specifier_std_string_view.data(), instance_specifier_std_string_view.size()};
    impl::tracing::ServiceElementInstanceIdentifierView service_element_instance_identifier_view_event{
        {service_type_name_, event_name_, impl::ServiceElementType::EVENT}, instance_specifier_string_view};
    const auto actual_service_instance_element_event =
        tracing_runtime.ConvertToTracingServiceInstanceElement(service_element_instance_identifier_view_event);

    impl::tracing::ServiceElementInstanceIdentifierView service_element_instance_identifier_view_field{
        {service_type_name_, field_name_, impl::ServiceElementType::FIELD}, instance_specifier_string_view};
    const auto actual_service_instance_element_field =
        tracing_runtime.ConvertToTracingServiceInstanceElement(service_element_instance_identifier_view_field);

    // Then the result should correspond to the provided configuration
    EXPECT_EQ(actual_service_instance_element_event, expected_service_instance_element_event);
    EXPECT_EQ(actual_service_instance_element_field, expected_service_instance_element_field);
}

using TracingRuntimeConvertToTracingServiceInstanceElementDeathFixture =
    TracingRuntimeConvertToTracingServiceInstanceElementFixture;
TEST_F(TracingRuntimeConvertToTracingServiceInstanceElementDeathFixture,
       CallingConvertFunctionOnElementWithoutInstanceIdTerminates)
{
    const auto service_identifier_type =
        make_ServiceIdentifierType(service_type_name_, major_version_number_, minor_version_number_);
    const LolaServiceInstanceDeployment lola_service_instance_deployment_without_instance_id{};
    const auto lola_service_type_deployment =
        CreateTypeDeployment(service_id_, {{event_name_, event_id_}}, {{field_name_, field_id_}});

    Configuration configuration{{{service_identifier_type, ServiceTypeDeployment{lola_service_type_deployment}}},
                                {{instance_specifier_,
                                  ServiceInstanceDeployment{service_identifier_type,
                                                            lola_service_instance_deployment_without_instance_id,
                                                            QualityType::kInvalid,
                                                            instance_specifier_}}},
                                GlobalConfiguration{},
                                TracingConfiguration{}};

    // Given a TracingRuntimeObject with a provided configuration object which does not contain an instance id
    TracingRuntime tracing_runtime{kNumberOfTotalConfiguredTracingSlots, configuration};

    // When converting a ServiceElementIdentifierView to a ServiceInstanceElement
    // Then we should terminate
    const auto instance_specifier_std_string_view = instance_specifier_.ToString();
    const auto instance_specifier_string_view =
        score::cpp::string_view{instance_specifier_std_string_view.data(), instance_specifier_std_string_view.size()};
    impl::tracing::ServiceElementInstanceIdentifierView service_element_instance_identifier_view_event{
        {service_type_name_, event_name_, impl::ServiceElementType::EVENT}, instance_specifier_string_view};
    EXPECT_DEATH(tracing_runtime.ConvertToTracingServiceInstanceElement(service_element_instance_identifier_view_event),
                 ".*");
}

TEST_F(TracingRuntimeConvertToTracingServiceInstanceElementDeathFixture,
       CallingConvertFunctionOnElementWithInvalidElementTypeTerminates)
{
    const auto service_identifier_type =
        make_ServiceIdentifierType(service_type_name_, major_version_number_, minor_version_number_);
    const LolaServiceInstanceDeployment lola_service_instance_deployment{LolaServiceInstanceId{instance_id_}};
    const auto lola_service_type_deployment =
        CreateTypeDeployment(service_id_, {{event_name_, event_id_}}, {{field_name_, field_id_}});

    Configuration configuration{
        {{service_identifier_type, ServiceTypeDeployment{lola_service_type_deployment}}},
        {{instance_specifier_,
          ServiceInstanceDeployment{
              service_identifier_type, lola_service_instance_deployment, QualityType::kInvalid, instance_specifier_}}},
        GlobalConfiguration{},
        TracingConfiguration{}};

    // Given a TracingRuntimeObject with a provided configuration object
    TracingRuntime tracing_runtime{kNumberOfTotalConfiguredTracingSlots, configuration};

    // When converting a ServiceElementIdentifierView with an invalid ServiceElementType to a ServiceInstanceElement
    // Then we should terminate
    const auto instance_specifier_std_string_view = instance_specifier_.ToString();
    const auto instance_specifier_string_view =
        score::cpp::string_view{instance_specifier_std_string_view.data(), instance_specifier_std_string_view.size()};
    impl::tracing::ServiceElementInstanceIdentifierView service_element_instance_identifier_view_event{
        {service_type_name_, event_name_, impl::ServiceElementType::INVALID}, instance_specifier_string_view};
    EXPECT_DEATH(tracing_runtime.ConvertToTracingServiceInstanceElement(service_element_instance_identifier_view_event),
                 ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola::tracing
