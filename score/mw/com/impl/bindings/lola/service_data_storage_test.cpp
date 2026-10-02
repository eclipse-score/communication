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
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/runtime_mock.h"
#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/new_delete_delegate_resource.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"
#include "score/os/ObjectSeam.h"
#include "score/os/mocklib/unistdmock.h"

#include <score/span.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sched.h>
#include <sys/types.h>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint64_t kFixtureMemoryResourceId{10U};
constexpr std::size_t kNumberOfServiceElements{4U};

// memory::DataTypeSizeInfo forbids constructing an instance with an alignment greater than
// alignof(std::max_align_t) ("overaligned" types are not supported), so all alignments used throughout this test
// file must not exceed this value.
constexpr std::size_t kMaxSupportedAlignment{alignof(std::max_align_t)};

const ElementFqId kElementFqId{1U, 1U, 1U, ServiceElementType::EVENT};

using ::testing::Return;

class ServiceDataStorageFixture : public ::testing::Test
{
  public:
    ServiceDataStorageFixture()
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(Return(&lola_runtime_mock_));
        ON_CALL(lola_runtime_mock_, GetPid()).WillByDefault(Return(pid_t{42}));
        ON_CALL(*unistd_mock_, getuid()).WillByDefault(Return(uid_t{42}));
    }

    ServiceDataStorage& GivenAServiceDataStorageWithCapacityOne()
    {
        service_data_storage_.emplace(1U, memory_resource_);
        return *service_data_storage_;
    }

    memory::shared::NewDeleteDelegateMemoryResource& GetMemoryResource() noexcept
    {
        return memory_resource_;
    }

    RuntimeMockGuard runtime_mock_guard_{};
    RuntimeMock lola_runtime_mock_{};
    os::MockGuard<os::UnistdMock> unistd_mock_{};

  private:
    memory::shared::NewDeleteDelegateMemoryResource memory_resource_{kFixtureMemoryResourceId};
    std::optional<ServiceDataStorage> service_data_storage_{std::nullopt};
};

TEST_F(ServiceDataStorageFixture, GetsPidFromUnistdAndStoresItOnConstruction)
{
    // Expecting that getpid will be called
    const pid_t pid{123};
    EXPECT_CALL(lola_runtime_mock_, GetPid()).WillOnce(Return(pid));

    // When creating a ServiceDataStorage
    const ServiceDataStorage unit{kNumberOfServiceElements, GetMemoryResource()};

    // Then the ServiceDataStorage will contain the returned PID
    EXPECT_EQ(unit.GetSkeletonPid(), pid);
}

TEST_F(ServiceDataStorageFixture, GetsUidFromRuntimAndStoresItOnConstruction)
{
    // Expecting that getuid will be called
    const uid_t uid{456};
    EXPECT_CALL(*unistd_mock_, getuid()).WillOnce(Return(uid));

    // When creating a ServiceDataStorage
    const ServiceDataStorage unit{kNumberOfServiceElements, GetMemoryResource()};

    // Then the ServiceDataStorage will contain the returned UID
    EXPECT_EQ(unit.GetSkeletonUid(), uid);
}

TEST_F(ServiceDataStorageFixture, GenericProxyEventMetaInfoIsStoredInServiceDataStorage)
{
    RecordProperty("Verifies", "SCR-32391303");
    RecordProperty("Description",
                   "Checks that the EventMataInfo is stored within ServiceDataStorage. Another test checks that "
                   "ServiceDataStorage is read-only.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDataStorage
    ServiceDataStorage& unit = GivenAServiceDataStorageWithCapacityOne();
    const memory::DataTypeSizeInfo sample_size_info{sizeof(std::uint32_t), alignof(std::uint32_t)};

    // When adding an event via AddEvent()
    unit.AddEvent(kElementFqId, SlotIndexType{1U}, sample_size_info);

    // Then its EventMetaInfo is stored and retrievable within the ServiceDataStorage
    const auto& event_meta_info = unit.GetEventMetaInfo(kElementFqId);
    EXPECT_EQ(event_meta_info.data_type_info_.Size(), sample_size_info.Size());
    EXPECT_EQ(event_meta_info.data_type_info_.Alignment(), sample_size_info.Alignment());
}

TEST_F(ServiceDataStorageFixture, AddEventConstructsEventDataStorageWithGivenNumberOfSlots)
{
    // Given a ServiceDataStorage with capacity for a single service-element
    ServiceDataStorage& unit = GivenAServiceDataStorageWithCapacityOne();
    const memory::DataTypeSizeInfo sample_size_info{sizeof(std::uint32_t), alignof(std::uint32_t)};
    constexpr SlotIndexType number_of_slots{5U};

    // When adding an event via AddEvent()
    auto& event_data_storage = unit.AddEvent(kElementFqId, number_of_slots, sample_size_info);

    // Then the returned EventDataStorage was constructed with the given number of slots
    EXPECT_EQ(event_data_storage.GetNumberOfSlots(), number_of_slots);
    // Note: Whether the slots are correctly sized is tested in EventDataStorage tests.
}

TEST_F(ServiceDataStorageFixture, AddEventRegistersEventDataStorageSoItCanBeFoundAgain)
{
    // Given a ServiceDataStorage with capacity for a single service-element
    ServiceDataStorage& unit = GivenAServiceDataStorageWithCapacityOne();
    const memory::DataTypeSizeInfo sample_size_info{sizeof(std::uint32_t), alignof(std::uint32_t)};

    // When adding an event via AddEvent()
    auto& event_data_storage = unit.AddEvent(kElementFqId, SlotIndexType{1U}, sample_size_info);

    // Then the very same EventDataStorage can be found again via GetEventDataStorage(), both via the non-const and
    // the const overload
    EXPECT_EQ(&unit.GetEventDataStorage(kElementFqId), &event_data_storage);
    EXPECT_EQ(&std::as_const(unit).GetEventDataStorage(kElementFqId), &event_data_storage);
}

using ServiceDataStorageDeathTest = ServiceDataStorageFixture;

TEST_F(ServiceDataStorageDeathTest, GetEventDataStorageTerminatesForUnregisteredElement)
{
    // Given a ServiceDataStorage without any registered events
    ServiceDataStorage& unit = GivenAServiceDataStorageWithCapacityOne();

    // When looking up an unregistered service-element via the non-const overload
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit.GetEventDataStorage(kElementFqId), ".*");
}

TEST_F(ServiceDataStorageDeathTest, GetEventDataStorageConstOverloadTerminatesForUnregisteredElement)
{
    // Given a ServiceDataStorage without any registered events
    const ServiceDataStorage& unit = GivenAServiceDataStorageWithCapacityOne();

    // When looking up an unregistered service-element via the const overload
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit.GetEventDataStorage(kElementFqId), ".*");
}

TEST_F(ServiceDataStorageDeathTest, GetEventMetaInfoTerminatesForUnregisteredElement)
{
    // Given a ServiceDataStorage without any registered events
    ServiceDataStorage& unit = GivenAServiceDataStorageWithCapacityOne();

    // When looking up an unregistered service-element
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit.GetEventMetaInfo(kElementFqId), ".*");
}

TEST(ServiceDataStorageShmSizeTest, IncreasingAlignedSlotArraySizeOfAServiceElementIncreasesCalculatedSize)
{
    // Given two sizing infos for a single service-element that only differ in the size of their raw slot-array
    const std::vector<EventDataStorageSizeInfo> service_elements_with_smaller_slot_array{
        EventDataStorageSizeInfo{2U, score::memory::DataTypeSizeInfo{32U, 16U}}};
    const std::vector<EventDataStorageSizeInfo> service_elements_with_bigger_slot_array{
        EventDataStorageSizeInfo{20U, score::memory::DataTypeSizeInfo{32U, 16U}}};

    // When calculating the required shm-size for both sizing infos
    const auto size_with_fewer_slots = CalculateServiceDataStorageShmSize(
        score::cpp::span<const EventDataStorageSizeInfo>{service_elements_with_smaller_slot_array});
    const auto size_with_more_slots = CalculateServiceDataStorageShmSize(
        score::cpp::span<const EventDataStorageSizeInfo>{service_elements_with_bigger_slot_array});

    // Then the calculated size for the service-element with the bigger raw slot-array is bigger.
    EXPECT_GT(size_with_more_slots, size_with_fewer_slots);
}

using EventsOrFieldsSizeInfo = std::vector<EventDataStorageSizeInfo>;

/// \brief Constructs a real ServiceDataStorage on the given resource and, for each entry of
/// service_elements_size_info, registers a real EventDataStorage via ServiceDataStorage::AddEvent() (exercising the
/// very same public API used by SkeletonMemoryManager at runtime for each event/field).
/// \return the number of bytes the given resource reports as allocated after construction.
std::size_t ConstructServiceDataStorageAndGetAllocatedBytes(const EventsOrFieldsSizeInfo& service_elements_size_info,
                                                            memory::shared::ManagedMemoryResource& resource)
{
    auto* const service_data_storage =
        resource.construct<ServiceDataStorage>(service_elements_size_info.size(), resource);

    ElementFqId::ElementId element_id{0U};
    for (const auto& service_element : service_elements_size_info)
    {
        // Each service-element (event/field) needs a unique ElementFqId to be registered under.
        const ElementFqId element_fq_id{1U, element_id, 1U, ServiceElementType::EVENT};
        ++element_id;

        score::cpp::ignore = service_data_storage->AddEvent(element_fq_id,
                                                            static_cast<SlotIndexType>(service_element.number_of_slots),
                                                            service_element.per_sample_size_info);
    }

    return resource.GetUserAllocatedBytes();
}

class ServiceDataStorageShmSizeParameterizedTestFixture : public ServiceDataStorageFixture,
                                                          public ::testing::WithParamInterface<EventsOrFieldsSizeInfo>
{
};

TEST_P(ServiceDataStorageShmSizeParameterizedTestFixture, CalculatedSizeMatchesActualAllocation)
{
    // Given the sizing information of some (possibly zero) service-elements (events/fields), each described by the
    // size/alignment of a single sample of its datatype plus its number of slots
    const auto& service_elements_size_info = GetParam();

    // When calculating the required shm-size for a ServiceDataStorage holding these service-elements
    const auto calculated_size = CalculateServiceDataStorageShmSize(
        score::cpp::span<const EventDataStorageSizeInfo>{service_elements_size_info});

    // Then the calculated size exactly matches the number of bytes actually allocated when constructing a real
    // ServiceDataStorage (and its EventDataStorages) with the very same sizing information.
    const auto actual_allocated_bytes =
        ConstructServiceDataStorageAndGetAllocatedBytes(service_elements_size_info, GetMemoryResource());

    EXPECT_EQ(calculated_size, actual_allocated_bytes);
}

INSTANTIATE_TEST_SUITE_P(
    ServiceDataStorageShmSizeTests,
    ServiceDataStorageShmSizeParameterizedTestFixture,
    ::testing::Values(
        // No service-elements at all (an empty span)
        EventsOrFieldsSizeInfo{},
        // A single service-element (event/field): 5 slots of a datatype of size/alignment 16
        EventsOrFieldsSizeInfo{EventDataStorageSizeInfo{5U, score::memory::DataTypeSizeInfo{16U, 16U}}},
        // Multiple service-elements (events/fields) with differing per-sample sizes/alignments and slot-counts
        EventsOrFieldsSizeInfo{
            EventDataStorageSizeInfo{2U, score::memory::DataTypeSizeInfo{8U, 8U}},
            EventDataStorageSizeInfo{14U,
                                     score::memory::DataTypeSizeInfo{kMaxSupportedAlignment, kMaxSupportedAlignment}},
            EventDataStorageSizeInfo{8U,
                                     score::memory::DataTypeSizeInfo{kMaxSupportedAlignment, kMaxSupportedAlignment}},
        }));

}  // namespace
}  // namespace score::mw::com::impl::lola
