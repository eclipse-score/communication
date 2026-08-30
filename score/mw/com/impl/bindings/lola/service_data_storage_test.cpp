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
#include <type_traits>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint64_t kMemoryResourceId{10U};
constexpr std::size_t kNumberOfServiceElements{4U};

// memory::DataTypeSizeInfo forbids constructing an instance with an alignment greater than
// alignof(std::max_align_t) ("overaligned" types are not supported), so all alignments used throughout this test
// file must not exceed this value.
constexpr std::size_t kMaxSupportedAlignment{alignof(std::max_align_t)};

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

    RuntimeMockGuard runtime_mock_guard_{};
    RuntimeMock lola_runtime_mock_{};
    os::MockGuard<os::UnistdMock> unistd_mock_{};
};

TEST(ServiceDataStorageTest, GenericProxyEventMetaInfoIsStoredInServiceDataStorage)
{
    RecordProperty("Verifies", "SCR-32391303");
    RecordProperty("Description",
                   "Checks that the EventMataInfo is stored within ServiceDataStorage. Another test checks that "
                   "ServiceDataStorage is read-only.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same_v<ServiceDataStorage::EventMetaInfoMap, decltype(ServiceDataStorage::events_metainfo_)>,
                  "ServiceDataStorage does not contain a map of EventMetaInfo.");
}

TEST_F(ServiceDataStorageFixture, GetsPidFromUnistdAndStoresItOnConstruction)
{
    // Expecting that getpid will be called
    const pid_t pid{123};
    EXPECT_CALL(lola_runtime_mock_, GetPid()).WillOnce(Return(pid));

    memory::shared::NewDeleteDelegateMemoryResource memory{kMemoryResourceId};
    // When creating a ServiceDataStorage
    const ServiceDataStorage unit{kNumberOfServiceElements, memory};

    // Then the ServiceDataStorage will contain the returned PID
    EXPECT_EQ(unit.skeleton_pid_, pid);
}

TEST_F(ServiceDataStorageFixture, GetsUidFromRuntimAndStoresItOnConstruction)
{
    // Expecting that getuid will be called
    const uid_t uid{456};
    EXPECT_CALL(*unistd_mock_, getuid()).WillOnce(Return(uid));

    memory::shared::NewDeleteDelegateMemoryResource memory{kMemoryResourceId};
    // When creating a ServiceDataStorage
    const ServiceDataStorage unit{kNumberOfServiceElements, memory};

    // Then the ServiceDataStorage will contain the returned UID
    EXPECT_EQ(unit.skeleton_uid_, uid);
}

TEST(ServiceDataStorageShmSizeTest, IncreasingAlignedSlotArraySizeOfAServiceElementIncreasesCalculatedSize)
{
    // Given two sizing infos for a single service-element that only differ in the size of their raw slot-array
    const std::vector<score::memory::DataTypeSizeInfo> service_elements_with_smaller_slot_array{
        score::memory::DataTypeSizeInfo{32U, 16U}};
    const std::vector<score::memory::DataTypeSizeInfo> service_elements_with_bigger_slot_array{
        score::memory::DataTypeSizeInfo{320U, 16U}};

    // When calculating the required shm-size for both sizing infos
    const auto size_with_fewer_slots = CalculateServiceDataStorageShmSize(
        score::cpp::span<const score::memory::DataTypeSizeInfo>{service_elements_with_smaller_slot_array});
    const auto size_with_more_slots = CalculateServiceDataStorageShmSize(
        score::cpp::span<const score::memory::DataTypeSizeInfo>{service_elements_with_bigger_slot_array});

    // Then the calculated size for the service-element with the bigger raw slot-array is bigger.
    EXPECT_GT(size_with_more_slots, size_with_fewer_slots);
}

TEST(ServiceDataStorageShmSizeTest, AddingAnAdditionalServiceElementIncreasesCalculatedSize)
{
    // Given the sizing information of one service-element and, additionally, the very same sizing information for
    // two service-elements
    const std::vector<score::memory::DataTypeSizeInfo> single_service_element{
        score::memory::DataTypeSizeInfo{48U, 16U}};
    const std::vector<score::memory::DataTypeSizeInfo> two_service_elements{score::memory::DataTypeSizeInfo{48U, 16U},
                                                                            score::memory::DataTypeSizeInfo{48U, 16U}};

    // When calculating the required shm-size for both sizing infos
    const auto size_for_single_service_element = CalculateServiceDataStorageShmSize(
        score::cpp::span<const score::memory::DataTypeSizeInfo>{single_service_element});
    const auto size_for_two_service_elements = CalculateServiceDataStorageShmSize(
        score::cpp::span<const score::memory::DataTypeSizeInfo>{two_service_elements});

    // Then the calculated size for two service-elements is bigger than for a single one.
    EXPECT_GT(size_for_two_service_elements, size_for_single_service_element);
}

/// \brief A trivial type of exactly Alignment bytes size, aligned to Alignment bytes.
/// \details Used to construct a real EventDataStorage<AlignedBlock<Alignment>> whose raw slot-array has the very
/// same size/alignment characteristics as a score::memory::DataTypeSizeInfo entry (Size() / Alignment()), by
/// choosing the number of slots as Size() / Alignment. This lets the tests below verify
/// CalculateServiceDataStorageShmSize's exact-size claim without needing to know the real (production) sample-type
/// of each service-element.
template <std::size_t Alignment>
struct alignas(Alignment) AlignedBlock
{
    std::byte data[Alignment];
};

/// \brief Constructs a real EventDataStorage<AlignedBlock<Alignment>> (with Size() / Alignment slots) on the
/// given resource, mirroring the size/alignment of the given service_element's raw slot-array.
template <std::size_t Alignment>
void ConstructEventDataStorage(const score::memory::DataTypeSizeInfo& service_element,
                               memory::shared::ManagedMemoryResource& resource)
{
    const auto number_of_slots = service_element.Size() / Alignment;
    score::cpp::ignore = resource.construct<EventDataStorage<AlignedBlock<Alignment>>>(
        number_of_slots, memory::shared::PolymorphicOffsetPtrAllocator<AlignedBlock<Alignment>>(resource));
}

/// \brief Constructs a real ServiceDataStorage on the given resource and, for each entry of
/// service_elements_size_info, a real EventDataStorage whose raw slot-array's size/alignment matches the entry
/// (mirroring what SkeletonMemoryManager does at runtime for each event/field).
/// \return the number of bytes the given resource reports as allocated after construction.
std::size_t ConstructServiceDataStorageAndGetAllocatedBytes(
    const std::vector<score::memory::DataTypeSizeInfo>& service_elements_size_info,
    memory::shared::ManagedMemoryResource& resource)
{
    score::cpp::ignore = resource.construct<ServiceDataStorage>(service_elements_size_info.size(), resource);

    for (const auto& service_element : service_elements_size_info)
    {
        // Only a handful of alignments are exercised by these tests; dispatch to the matching instantiation of
        // ConstructEventDataStorage() so a real DynamicArray with the required alignment gets allocated.
        switch (service_element.Alignment())
        {
            case 8U:
                ConstructEventDataStorage<8U>(service_element, resource);
                break;
            case kMaxSupportedAlignment:
                ConstructEventDataStorage<kMaxSupportedAlignment>(service_element, resource);
                break;
            default:
                ADD_FAILURE() << "Unsupported alignment in test: " << service_element.Alignment();
                break;
        }
    }

    return resource.GetUserAllocatedBytes();
}

using ServiceElementsSizeInfo = std::vector<score::memory::DataTypeSizeInfo>;

class ServiceDataStorageShmSizeParameterizedTestFixture : public ServiceDataStorageFixture,
                                                          public ::testing::WithParamInterface<ServiceElementsSizeInfo>
{
};

TEST_P(ServiceDataStorageShmSizeParameterizedTestFixture, CalculatedSizeMatchesActualAllocation)
{
    // Given the sizing information of some (possibly zero) service-elements (events/fields)
    const auto& service_elements_size_info = GetParam();

    // When calculating the required shm-size for a ServiceDataStorage holding these service-elements
    const auto calculated_size = CalculateServiceDataStorageShmSize(
        score::cpp::span<const score::memory::DataTypeSizeInfo>{service_elements_size_info});

    // Then the calculated size exactly matches the number of bytes actually allocated when constructing a real
    // ServiceDataStorage (and its EventDataStorages) with the very same sizing information.
    memory::shared::NewDeleteDelegateMemoryResource resource{kMemoryResourceId};
    const auto actual_allocated_bytes =
        ConstructServiceDataStorageAndGetAllocatedBytes(service_elements_size_info, resource);

    EXPECT_EQ(calculated_size, actual_allocated_bytes);
}

INSTANTIATE_TEST_SUITE_P(
    ServiceDataStorageShmSizeTests,
    ServiceDataStorageShmSizeParameterizedTestFixture,
    ::testing::Values(
        // No service-elements at all (an empty span)
        ServiceElementsSizeInfo{},
        // A single service-element (event/field)
        ServiceElementsSizeInfo{score::memory::DataTypeSizeInfo{80U, 16U}},
        // Multiple service-elements (events/fields) with differing raw slot-array sizes and alignments
        ServiceElementsSizeInfo{
            score::memory::DataTypeSizeInfo{16U, 8U},
            score::memory::DataTypeSizeInfo{224U, kMaxSupportedAlignment},
            score::memory::DataTypeSizeInfo{128U, kMaxSupportedAlignment},
        }));

}  // namespace
}  // namespace score::mw::com::impl::lola
