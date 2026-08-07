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
#include "score/mw/com/impl/bindings/lola/event_meta_info.h"
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
#include <type_traits>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint64_t kMemoryResourceId{10U};
constexpr std::size_t kNumberOfServiceElements{4U};

using namespace ::testing;

class ServiceDataStorageFixture : public ::testing::Test
{
  public:
    ServiceDataStorageFixture()
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(::testing::Return(&lola_runtime_mock_));
    }

    RuntimeMockGuard runtime_mock_guard_{};
    RuntimeMock lola_runtime_mock_{};
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

// When compiling for linux, the underlying container allocator differs. Since the tests for satisfying requirements
// must be on qnx, we only check the service element event map type on qnx.
#if !defined(__linux__)
TEST_F(ServiceDataStorageFixture, ServiceElementsAreIndexedUsingElementFqId)
{
    RecordProperty("Verifies", "SCR-21555839");
    RecordProperty("Description",
                   "Checks that service elements are stored in a fixed-capacity LinearSearchMap within "
                   "ServiceDataStorage. The LinearSearchMap resolves a key via find() by comparing it against the "
                   "stored keys and returns the value corresponding to the provided key and not any other key. "
                   "Therefore, resolving a service element from an EventFqId will never return the wrong storage "
                   "location for the service element. Unlike dynamically allocating map-types, its memory footprint is "
                   "deterministic and can be calculated up-front without a simulation run.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    memory::shared::NewDeleteDelegateMemoryResource memory{kMemoryResourceId};
    const ServiceDataStorage unit{kNumberOfServiceElements, memory};
    using ActualEventMapType = decltype(unit.events_);

    using ExpectedKeyType = ElementFqId;
    using ExpectedValueType = score::memory::shared::OffsetPtr<void>;
    using ExpectedEventMapType = LinearSearchMap<ExpectedKeyType, ExpectedValueType>;

    static_assert(std::is_same_v<ActualEventMapType, ExpectedEventMapType>, "Event map is not a LinearSearchMap");
}
#endif  // not __linux__

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
    const os::MockGuard<os::UnistdMock> unistd_mock{};

    // Expecting that getuid will be called
    const uid_t uid{456};
    EXPECT_CALL(*unistd_mock, getuid()).WillOnce(Return(uid));

    memory::shared::NewDeleteDelegateMemoryResource memory{kMemoryResourceId};
    // When creating a ServiceDataStorage
    const ServiceDataStorage unit{kNumberOfServiceElements, memory};

    // Then the ServiceDataStorage will contain the returned UID
    EXPECT_EQ(unit.skeleton_uid_, uid);
}

class ServiceDataStorageShmSizeFixture : public ServiceDataStorageFixture
{
  public:
    /// \brief Constructs a real ServiceDataStorage on the given resource and populates its events_ / events_metainfo_
    /// with one EventDataStorage (and corresponding EventMetaInfo) per entry of service_elements_size_info, mirroring
    /// what SkeletonMemoryManager::CreateGenericEventDataInCreatedSharedMemory does at runtime.
    /// \return the number of bytes the given resource reports as allocated after construction.
    std::size_t ConstructServiceDataStorageAndGetAllocatedBytes(
        const std::vector<ServiceElementDataStorageSizeInfo>& service_elements_size_info,
        memory::shared::ManagedMemoryResource& resource)
    {
        auto& storage = *resource.construct<ServiceDataStorage>(service_elements_size_info.size(), resource);

        std::uint16_t element_id{0U};
        for (const auto& service_element : service_elements_size_info)
        {
            const ElementFqId element_fq_id{1U, element_id, 1U, ServiceElementType::EVENT};

            // Mirrors SkeletonMemoryManager::CreateGenericEventDataInCreatedSharedMemory: the total number of bytes
            // for all slots is converted into a (rounded up) number of std::max_align_t elements.
            const std::size_t total_data_size_bytes =
                service_element.number_of_slots * service_element.aligned_slot_size;
            const std::size_t num_max_align_elements =
                (total_data_size_bytes + sizeof(std::max_align_t) - 1U) / sizeof(std::max_align_t);

            auto* const data_storage = resource.construct<EventDataStorage<std::max_align_t>>(
                num_max_align_elements, memory::shared::PolymorphicOffsetPtrAllocator<std::max_align_t>(resource));

            score::cpp::ignore = storage.events_.emplace(
                std::piecewise_construct, std::forward_as_tuple(element_fq_id), std::forward_as_tuple(data_storage));

            // The exact sample_size / sample_alignment values are irrelevant for the shm-layout (only the
            // number_of_slots / aligned_slot_size of the slot-array matter), so we simply reuse aligned_slot_size
            // with an alignment of 1 (which trivially satisfies DataTypeSizeInfo's "size is a multiple of alignment"
            // precondition).
            const memory::DataTypeSizeInfo sample_meta_info{service_element.aligned_slot_size, 1U};
            score::cpp::ignore =
                storage.events_metainfo_.emplace(std::piecewise_construct,
                                                 std::forward_as_tuple(element_fq_id),
                                                 std::forward_as_tuple(sample_meta_info, data_storage->data()));
            ++element_id;
        }

        return resource.GetUserAllocatedBytes();
    }

    /// \brief Computes the number of individual (bump-)allocations performed by the (monotonic) memory resource while
    /// constructing a ServiceDataStorage (and its EventDataStorages) for the given service_elements_size_info.
    /// \details Mirrors the allocation sites accounted for by CalculateServiceDataStorageShmSize: the
    /// ServiceDataStorage object itself and the two LinearSearchMap backing arrays (3 allocations, independent of the
    /// number of service-elements), and, per service-element, the EventDataStorage object and its raw slot-array (2
    /// allocations).
    static std::size_t ComputeNumberOfAllocations(
        const std::vector<ServiceElementDataStorageSizeInfo>& service_elements_size_info)
    {
        constexpr std::size_t kNumberOfFixedAllocations{3U};
        constexpr std::size_t kNumberOfAllocationsPerServiceElement{2U};
        return kNumberOfFixedAllocations + (kNumberOfAllocationsPerServiceElement * service_elements_size_info.size());
    }

    /// \brief Asserts that calculated_size is sufficient (i.e. not smaller than actual_allocated_bytes) but also not
    ///        an arbitrarily/excessively large over-estimation of the real allocation.
    /// \details CalculateServiceDataStorageShmSize rounds every individual allocation up to
    ///          alignof(std::max_align_t), whereas the real (monotonic) memory resource only pads as much as the
    ///          current (running) offset actually requires (between 0 and alignof(std::max_align_t) - 1 bytes). Hence
    ///          the only legitimate source of waste between the calculated and the actual size is this per-allocation
    ///          alignment padding, bounded by (alignof(std::max_align_t) - 1) * number_of_allocations. Should the
    ///          calculation be egregiously wrong (e.g. by a missing or wrongly-multiplied term), the resulting size
    ///          would exceed this bound.
    /// \attention If this expectation fails, check whether CalculateServiceDataStorageShmSize implementation has
    /// changed
    ///            and handles padding/alignment differently!
    static void ExpectCalculatedSizeIsSufficientAndNotExcessive(
        const std::vector<ServiceElementDataStorageSizeInfo>& service_elements_size_info,
        const std::size_t calculated_size,
        const std::size_t actual_allocated_bytes)
    {
        EXPECT_GE(calculated_size, actual_allocated_bytes);

        const std::size_t max_possible_alignment_padding =
            (alignof(std::max_align_t) - 1U) * ComputeNumberOfAllocations(service_elements_size_info);
        EXPECT_LE(calculated_size, actual_allocated_bytes + max_possible_alignment_padding);
    }
};

TEST_F(ServiceDataStorageShmSizeFixture, EmptyServiceElementsYieldsSizeOfServiceDataStorageWithoutEventStorages)
{
    // Given no service-elements at all (an empty span)
    const score::cpp::span<const ServiceElementDataStorageSizeInfo> empty_service_elements{};

    // When calculating the required shm-size for a ServiceDataStorage
    const auto calculated_size = CalculateServiceDataStorageShmSize(empty_service_elements);

    // Then the calculated size is sufficient for (i.e. greater than or equal to) the number of bytes actually
    // allocated when constructing a real ServiceDataStorage with zero service-elements on the very same kind of
    // (monotonic bump) memory resource, and it does not exceed the actual allocation by more than the maximum
    // possible per-allocation alignment padding.
    memory::shared::NewDeleteDelegateMemoryResource resource{kMemoryResourceId};
    const auto actual_allocated_bytes = ConstructServiceDataStorageAndGetAllocatedBytes({}, resource);

    ExpectCalculatedSizeIsSufficientAndNotExcessive({}, calculated_size, actual_allocated_bytes);
}

TEST_F(ServiceDataStorageShmSizeFixture, SingleServiceElementCalculatedSizeIsSufficientForActualAllocation)
{
    // Given the sizing information of a single service-element (event/field)
    const std::vector<ServiceElementDataStorageSizeInfo> service_elements_size_info{
        ServiceElementDataStorageSizeInfo{5U, 16U}};

    // When calculating the required shm-size for a ServiceDataStorage holding this single service-element
    const auto calculated_size = CalculateServiceDataStorageShmSize(
        score::cpp::span<const ServiceElementDataStorageSizeInfo>{service_elements_size_info});

    // Then the calculated size is sufficient for (i.e. greater than or equal to) the number of bytes actually
    // allocated when constructing a real ServiceDataStorage (and its single EventDataStorage) with the very same
    // sizing information, and it does not exceed the actual allocation by more than the maximum possible
    // per-allocation alignment padding (see class-level documentation of CalculateServiceDataStorageShmSize).
    memory::shared::NewDeleteDelegateMemoryResource resource{kMemoryResourceId};
    const auto actual_allocated_bytes =
        ConstructServiceDataStorageAndGetAllocatedBytes(service_elements_size_info, resource);

    ExpectCalculatedSizeIsSufficientAndNotExcessive(
        service_elements_size_info, calculated_size, actual_allocated_bytes);
}

TEST_F(ServiceDataStorageShmSizeFixture,
       MultipleServiceElementsWithDifferingSizesCalculatedSizeIsSufficientForActualAllocation)
{
    // Given the sizing information of multiple service-elements (events/fields) with differing numbers of slots and
    // differing (already aligned) slot sizes
    const std::vector<ServiceElementDataStorageSizeInfo> service_elements_size_info{
        ServiceElementDataStorageSizeInfo{2U, 8U},
        ServiceElementDataStorageSizeInfo{7U, 32U},
        ServiceElementDataStorageSizeInfo{1U, 64U},
    };

    // When calculating the required shm-size for a ServiceDataStorage holding these service-elements
    const auto calculated_size = CalculateServiceDataStorageShmSize(
        score::cpp::span<const ServiceElementDataStorageSizeInfo>{service_elements_size_info});

    // Then the calculated size is sufficient for (i.e. greater than or equal to) the number of bytes actually
    // allocated when constructing a real ServiceDataStorage (and its EventDataStorages) with the very same sizing
    // information, and it does not exceed the actual allocation by more than the maximum possible per-allocation
    // alignment padding.
    memory::shared::NewDeleteDelegateMemoryResource resource{kMemoryResourceId};
    const auto actual_allocated_bytes =
        ConstructServiceDataStorageAndGetAllocatedBytes(service_elements_size_info, resource);

    ExpectCalculatedSizeIsSufficientAndNotExcessive(
        service_elements_size_info, calculated_size, actual_allocated_bytes);
}

TEST(ServiceDataStorageShmSizeTest, IncreasingNumberOfSlotsOfAServiceElementIncreasesCalculatedSize)
{
    // Given two sizing infos for a single service-element that only differ in their number of slots
    const std::vector<ServiceElementDataStorageSizeInfo> service_elements_with_fewer_slots{
        ServiceElementDataStorageSizeInfo{2U, 16U}};
    const std::vector<ServiceElementDataStorageSizeInfo> service_elements_with_more_slots{
        ServiceElementDataStorageSizeInfo{20U, 16U}};

    // When calculating the required shm-size for both sizing infos
    const auto size_with_fewer_slots = CalculateServiceDataStorageShmSize(
        score::cpp::span<const ServiceElementDataStorageSizeInfo>{service_elements_with_fewer_slots});
    const auto size_with_more_slots = CalculateServiceDataStorageShmSize(
        score::cpp::span<const ServiceElementDataStorageSizeInfo>{service_elements_with_more_slots});

    // Then the calculated size for the service-element with more slots is bigger, since each slot requires additional
    // space in the event's raw data slot-array.
    EXPECT_GT(size_with_more_slots, size_with_fewer_slots);
}

TEST(ServiceDataStorageShmSizeTest, IncreasingAlignedSlotSizeOfAServiceElementIncreasesCalculatedSize)
{
    // Given two sizing infos for a single service-element that only differ in their (already aligned) per-slot size
    const std::vector<ServiceElementDataStorageSizeInfo> service_elements_with_smaller_slot_size{
        ServiceElementDataStorageSizeInfo{2U, 8U}};
    const std::vector<ServiceElementDataStorageSizeInfo> service_elements_with_bigger_slot_size{
        ServiceElementDataStorageSizeInfo{2U, 128U}};

    // When calculating the required shm-size for both sizing infos
    const auto size_with_smaller_slot_size = CalculateServiceDataStorageShmSize(
        score::cpp::span<const ServiceElementDataStorageSizeInfo>{service_elements_with_smaller_slot_size});
    const auto size_with_bigger_slot_size = CalculateServiceDataStorageShmSize(
        score::cpp::span<const ServiceElementDataStorageSizeInfo>{service_elements_with_bigger_slot_size});

    // Then the calculated size for the service-element with the bigger per-slot size is bigger, since each of its
    // slots occupies more space in the event's raw data slot-array.
    EXPECT_GT(size_with_bigger_slot_size, size_with_smaller_slot_size);
}

TEST(ServiceDataStorageShmSizeTest, AddingAnAdditionalServiceElementIncreasesCalculatedSize)
{
    // Given the sizing information of one service-element and, additionally, the very same sizing information for
    // two service-elements
    const std::vector<ServiceElementDataStorageSizeInfo> single_service_element{
        ServiceElementDataStorageSizeInfo{3U, 16U}};
    const std::vector<ServiceElementDataStorageSizeInfo> two_service_elements{
        ServiceElementDataStorageSizeInfo{3U, 16U}, ServiceElementDataStorageSizeInfo{3U, 16U}};

    // When calculating the required shm-size for both sizing infos
    const auto size_for_single_service_element = CalculateServiceDataStorageShmSize(
        score::cpp::span<const ServiceElementDataStorageSizeInfo>{single_service_element});
    const auto size_for_two_service_elements = CalculateServiceDataStorageShmSize(
        score::cpp::span<const ServiceElementDataStorageSizeInfo>{two_service_elements});

    // Then the calculated size for two service-elements is bigger than for a single one.
    EXPECT_GT(size_for_two_service_elements, size_for_single_service_element);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
