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
#include "score/memory/shared/memory_region_map.h"

#include "memory_region_bounds.h"
#include "score/concurrency/atomic_indirector.h"
#include "score/concurrency/atomic_mock.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace score::memory::shared::test
{

using ::testing::_;
using ::testing::Return;

using score::concurrency::AtomicIndirectorMock;
using score::concurrency::AtomicMock;

class MemoryRegionMapAttorney
{
  public:
    using MemoryRegionMapMock = detail::MemoryRegionMapImpl<concurrency::AtomicIndirectorMock>;
    using RegionVersionRefCountType = MemoryRegionMapMock::RegionVersionRefCountType;

    constexpr static auto VERSION_COUNT = MemoryRegionMapMock::VERSION_COUNT;
    constexpr static auto INVALID_REF_COUNT_INTERVAL_START = MemoryRegionMapMock::INVALID_REF_COUNT_INTERVAL_START;
    constexpr static auto INVALID_REF_COUNT_INTERVAL_END = MemoryRegionMapMock::INVALID_REF_COUNT_INTERVAL_END;

    MemoryRegionMapAttorney(MemoryRegionMapMock& memory_region_map) noexcept : memory_region_map_{memory_region_map} {}

    std::optional<std::uint8_t> AcquireRegionVersionForOverwrite() noexcept
    {
        return memory_region_map_.AcquireRegionVersionForOverwrite();
    }

    std::optional<typename MemoryRegionMapMock::AcquiredRefcountIndex> AcquireLatestRegionVersionForRead()
        const noexcept
    {
        return memory_region_map_.AcquireLatestRegionVersionForRead();
    }

    void SetAllSlotRefCountsToZero() noexcept
    {
        for (auto& slot_ref_count : memory_region_map_.known_regions_versions_refcounts_)
        {
            slot_ref_count = 0U;
        }
    }

  private:
    MemoryRegionMapMock& memory_region_map_;
};

class MemoryRegionMapTest : public ::testing::Test
{
  public:
    MemoryRegionMapTest() = default;

    MemoryRegionMap unit_{};
};

class MockMemoryRegionMapTest : public MemoryRegionMapTest
{
  protected:
    using MemoryRegionMapMock = detail::MemoryRegionMapImpl<concurrency::AtomicIndirectorMock>;
    using AtomicType = MemoryRegionMapAttorney::RegionVersionRefCountType;

    MockMemoryRegionMapTest() : unit_{}, attorney_{unit_}, atomic_mock_{}
    {
        concurrency::AtomicIndirectorMock<AtomicType>::SetMockObject(&atomic_mock_);

        // We set the ref count of all versions to 0 because the version acquisition algorithm treats unused versions
        // (i.e. versions which still have the default initial ref count value) differently.
        attorney_.SetAllSlotRefCountsToZero();
    }

    ~MockMemoryRegionMapTest()
    {
        concurrency::AtomicIndirectorMock<AtomicType>::SetMockObject(nullptr);
    }

    void ExpectAcquireRegionVersionForOverwriteCannotAcquireRegion(bool is_death_test = false)
    {
        const std::uint8_t max_retries = 10U;

        // When iterating over the versions to find one available for writing, we skip the current latest know version.
        // If the update of a version's ref count fails, then we try the next version. Therefore, if the update fails
        // every time, we could only ever attempt to update (VERSION_COUNT - 1) versions per try.
        const std::uint16_t total_calls = (MemoryRegionMapAttorney::VERSION_COUNT - 1) * max_retries;

        // Given that the operation to update the chosen version's ref count to indicate that it is being currently
        // written to fails every time (because a reader incremented its ref count between the writer thread loading the
        // ref count and updating its value),
        if (is_death_test)
        {
            EXPECT_CALL(atomic_mock_, compare_exchange_weak(_, _, _))
                .Times(::testing::AtMost(total_calls))
                .WillRepeatedly(Return(false));
        }
        else
        {
            EXPECT_CALL(atomic_mock_, compare_exchange_weak(_, _, _)).Times(total_calls).WillRepeatedly(Return(false));
        }
    }

    void ExpectAcquireRegionVersionForReadCannotAcquireRegion(bool is_death_test = false)
    {
        const std::uint8_t max_retries = 255U;

        // Given that the operation to update the chosen version's ref count to indicate that it is being currently
        // read returns a value indicating that it is already being written to,
        if (is_death_test)
        {
            EXPECT_CALL(atomic_mock_, fetch_add(_, _))
                .Times(::testing::AtMost(max_retries))
                .WillRepeatedly(Return(MemoryRegionMapAttorney::INVALID_REF_COUNT_INTERVAL_START));
        }
        else
        {
            EXPECT_CALL(atomic_mock_, fetch_add(_, _))
                .Times(max_retries)
                .WillRepeatedly(Return(MemoryRegionMapAttorney::INVALID_REF_COUNT_INTERVAL_START));
        }
    }

    MemoryRegionMapMock unit_;
    MemoryRegionMapAttorney attorney_;
    concurrency::AtomicMock<AtomicType> atomic_mock_;
};

TEST_F(MemoryRegionMapTest, ReturnsNullMemoryBoundsIfKnownRegionsEmpty)
{
    // Given an empty MemoryRegionMap
    // When checking the memory bounds for a pointer
    const auto foundMemoryBounds = unit_.GetBoundsFromAddress(std::uintptr_t{50U});

    // Then null memory bounds should be returned
    EXPECT_FALSE(foundMemoryBounds.has_value());
}

TEST_F(MemoryRegionMapTest, ReturnsMemoryBoundsForPointersInBounds)
{
    const MemoryRegionBounds firstMemoryBounds{50U, 100};
    const MemoryRegionBounds secondMemoryBounds{150U, 200};

    // Given 2 memory ranges are inserted into the MemoryRegionMap
    EXPECT_TRUE(unit_.UpdateKnownRegion(firstMemoryBounds.GetStartAddress(), firstMemoryBounds.GetEndAddress()));
    EXPECT_TRUE(unit_.UpdateKnownRegion(secondMemoryBounds.GetStartAddress(), secondMemoryBounds.GetEndAddress()));

    // When checking the memory bounds for pointers inside the memory bounds
    const auto firstFoundMemoryBounds0 = unit_.GetBoundsFromAddress(std::uintptr_t{50});
    const auto firstFoundMemoryBounds1 = unit_.GetBoundsFromAddress(std::uintptr_t{75});
    const auto firstFoundMemoryBounds2 = unit_.GetBoundsFromAddress(std::uintptr_t{100});
    const auto secondFoundMemoryBounds0 = unit_.GetBoundsFromAddress(std::uintptr_t{150});
    const auto secondFoundMemoryBounds1 = unit_.GetBoundsFromAddress(std::uintptr_t{175});
    const auto secondFoundMemoryBounds2 = unit_.GetBoundsFromAddress(std::uintptr_t{200});

    const auto notFoundMemoryBounds = unit_.GetBoundsFromAddress(std::uintptr_t{500});

    // Then the correct bounds should be returned
    ASSERT_TRUE(firstFoundMemoryBounds0.has_value());
    ASSERT_TRUE(firstFoundMemoryBounds1.has_value());
    ASSERT_TRUE(firstFoundMemoryBounds2.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds0.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds1.has_value());
    ASSERT_TRUE(secondFoundMemoryBounds2.has_value());

    EXPECT_EQ(firstFoundMemoryBounds0.value(), firstMemoryBounds);
    EXPECT_EQ(firstFoundMemoryBounds1.value(), firstMemoryBounds);
    EXPECT_EQ(firstFoundMemoryBounds2.value(), firstMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds0.value(), secondMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds1.value(), secondMemoryBounds);
    EXPECT_EQ(secondFoundMemoryBounds2.value(), secondMemoryBounds);

    EXPECT_FALSE(notFoundMemoryBounds.has_value());
}

class MemoryRegionMapUpdateRegionParamaterisedFixture
    : public ::testing::TestWithParam<std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>>
{
};

TEST_P(MemoryRegionMapUpdateRegionParamaterisedFixture,
       UpdateKnownRegionFailsIfProvidedMemoryRangeOverlapsWithExistingRange)
{
    MemoryRegionMap unit{};
    const auto ranges_to_insert = GetParam();

    for (const auto& range_pair : ranges_to_insert)
    {
        const auto& range_to_insert = range_pair.first;
        const bool should_update_succeed = range_pair.second;

        EXPECT_EQ(unit.UpdateKnownRegion(range_to_insert.first, range_to_insert.second), should_update_succeed);
    }
}

INSTANTIATE_TEST_SUITE_P(MemoryRegionMapUpdateRegionParamaterisedFixture,
                         MemoryRegionMapUpdateRegionParamaterisedFixture,
                         ::testing::Values(
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{0x50U}, std::uintptr_t{0x100U}}, true},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{0x50U}, std::uintptr_t{0x100U}}, true},
                                 {{std::uintptr_t{0x150U}, std::uintptr_t{0x200U}}, true},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{0x50U}, std::uintptr_t{0x100U}}, true},
                                 {{std::uintptr_t{0x100U}, std::uintptr_t{0x200U}}, true},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{0x100U}, std::uintptr_t{0x200U}}, true},
                                 {{std::uintptr_t{0x50U}, std::uintptr_t{0x100U}}, true},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{0x50U}, std::uintptr_t{0x100U}}, true},
                                 {{std::uintptr_t{0x10U}, std::uintptr_t{0x40U}}, true},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{50}, std::uintptr_t{100}}, true},
                                 {{std::uintptr_t{10}, std::uintptr_t{60}}, false},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{50}, std::uintptr_t{100}}, true},
                                 {{std::uintptr_t{80}, std::uintptr_t{150}}, false},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{50}, std::uintptr_t{100}}, true},
                                 {{std::uintptr_t{80}, std::uintptr_t{150}}, false},
                                 {{std::uintptr_t{120}, std::uintptr_t{200}}, true},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{50}, std::uintptr_t{100}}, true},
                                 {{std::uintptr_t{200}, std::uintptr_t{250}}, true},
                                 {{std::uintptr_t{180}, std::uintptr_t{220}}, false},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{50}, std::uintptr_t{100}}, true},
                                 {{std::uintptr_t{200}, std::uintptr_t{250}}, true},
                                 {{std::uintptr_t{80}, std::uintptr_t{180}}, false},
                             },
                             std::vector<std::pair<std::pair<std::uintptr_t, std::uintptr_t>, bool>>{
                                 {{std::uintptr_t{50}, std::uintptr_t{100}}, true},
                                 {{std::uintptr_t{200}, std::uintptr_t{250}}, true},
                                 {{std::uintptr_t{80}, std::uintptr_t{280}}, false},
                             }));

TEST_F(MemoryRegionMapTest, GetBoundsFromAddressWillNotReturnRangeForRegionWhichFailedToInsert)
{
    const MemoryRegionBounds validMemoryBounds{50U, 100U};
    const MemoryRegionBounds invalidMemoryBounds{10U, 60U};

    // Given a memory range is inserted into the MemoryRegionMap
    EXPECT_TRUE(unit_.UpdateKnownRegion(validMemoryBounds.GetStartAddress(), validMemoryBounds.GetEndAddress()));

    // When inserting a memory region which overlaps with the existing memory range
    // Then the region cannot be inserted
    EXPECT_FALSE(unit_.UpdateKnownRegion(invalidMemoryBounds.GetStartAddress(), invalidMemoryBounds.GetEndAddress()));

    // and when calling GetBoundsFromAddress for a value within the invalid range but not within the valid range
    // Then an empty optional should be returned
    EXPECT_FALSE(unit_.GetBoundsFromAddress(std::uintptr_t{40U}).has_value());
}

TEST_F(MemoryRegionMapTest, InsertAndRemove)
{
    const MemoryRegionBounds memoryBounds{50U, 100U};

    // Given a memory region is inserted into the MemoryRegionMap
    EXPECT_TRUE(unit_.UpdateKnownRegion(memoryBounds.GetStartAddress(), memoryBounds.GetEndAddress()));

    // When checking the memory bounds for pointers inside the memory regions
    auto foundMemoryBounds = unit_.GetBoundsFromAddress(std::uintptr_t{50U});

    // Then the correct bounds should be returned
    ASSERT_TRUE(foundMemoryBounds.has_value());
    EXPECT_EQ(foundMemoryBounds.value(), memoryBounds);

    // ... and when removing the memory bounds again
    unit_.RemoveKnownRegion(memoryBounds.GetStartAddress());

    // and when checking memory bounds for pointers inside the memory regions
    foundMemoryBounds = unit_.GetBoundsFromAddress(std::uintptr_t{50U});

    // then nothing should be found
    EXPECT_FALSE(foundMemoryBounds.has_value());
}

TEST_F(MemoryRegionMapTest, Clear)
{
    const MemoryRegionBounds firstMemoryBounds{50U, 100U};
    const MemoryRegionBounds secondMemoryBounds{150U, 200U};

    // Given 2 memory ranges are inserted into the MemoryRegionMap
    EXPECT_TRUE(unit_.UpdateKnownRegion(firstMemoryBounds.GetStartAddress(), firstMemoryBounds.GetEndAddress()));
    EXPECT_TRUE(unit_.UpdateKnownRegion(secondMemoryBounds.GetStartAddress(), secondMemoryBounds.GetEndAddress()));

    // and when we clear the map
    unit_.ClearKnownRegions();

    // and then check for the bounds of the previously inserted regions
    const auto firstFoundMemoryBounds = unit_.GetBoundsFromAddress(firstMemoryBounds.GetStartAddress());
    const auto secondFoundMemoryBounds = unit_.GetBoundsFromAddress(secondMemoryBounds.GetStartAddress());

    // Then the regions shouldn't be there
    EXPECT_FALSE(firstFoundMemoryBounds.has_value());
    EXPECT_FALSE(secondFoundMemoryBounds.has_value());
}

/// \brief multi-threaded test case with a writer changing the known_regions and N readers doing bounds-lookups.
/// \details The test is split into two clearly separated phases, joined by a barrier: an insertion phase (the writer
///          inserts all 100 regions, one at a time, in index order) and a removal phase (the writer removes them all,
///          one at a time, in the same order). For each phase the writer publishes its progress through a pair of
///          atomic counters - "started index" (stored with release, right before touching the map) and "finished
///          index" (stored with release, right after). Readers take acquire loads of these counters immediately
///          before/after their own GetBoundsFromAddress() call - which remains completely unsynchronized/lock-free
///          and use the resulting release/acquire happens-before relationship (not a timing assumption) to determine
///          whether a lookup has a provably definite expected answer:
///          - if a region's insertion/removal was already finished before the lookup even started, the outcome is
///            asserted.
///          - if it hadn't even started yet, even after the lookup finished, the outcome is asserted too.
///          - otherwise the writer genuinely overlapped with the lookup, so no plausibility assertion is made
///            (though a region that IS found must always resolve to the correct bounds).
TEST_F(MemoryRegionMapTest, ConcurrentAccess)
{
    using namespace std::chrono_literals;

    constexpr std::size_t kNumRegions{100U};
    constexpr std::size_t kNumReaderThreads{4U};
    // each with a size of 50 bytes
    constexpr std::uint8_t MEM_REGION_SIZE{50U};

    // given 100 memory regions
    std::vector<MemoryRegionBounds> memory_regions;
    memory_regions.reserve(kNumRegions);
    for (unsigned int i = 0U; i < kNumRegions; i++)
    {
        memory_regions.emplace_back(static_cast<uintptr_t>(i * 100U + 1U),
                                    static_cast<uintptr_t>(i * 100U + MEM_REGION_SIZE));
    }

    std::atomic<std::size_t> insert_started{0U};
    std::atomic<std::size_t> insert_finished{0U};
    std::atomic<std::size_t> remove_started{0U};
    std::atomic<std::size_t> remove_finished{0U};

    // Single-use rendezvous point between the insertion and removal phases: nobody (writer or readers) starts acting
    // on/asserting about removals until everybody has finished acting on/asserting about insertions.
    std::mutex barrier_mutex;
    std::condition_variable barrier_cv;
    std::size_t threads_at_barrier{0U};
    constexpr std::size_t kNumParticipants{kNumReaderThreads + 1U};
    auto arrive_and_wait = [&]() {
        std::unique_lock<std::mutex> lock{barrier_mutex};
        ++threads_at_barrier;
        if (threads_at_barrier == kNumParticipants)
        {
            barrier_cv.notify_all();
        }
        else
        {
            barrier_cv.wait(lock, [&] {
                return threads_at_barrier == kNumParticipants;
            });
        }
    };

    // and one writer thread, which first inserts and afterwards removes these memory regions
    auto writer_activity = [&]() {
        for (std::size_t index = 0U; index < memory_regions.size(); ++index)
        {
            const auto& reg = memory_regions[index];
            insert_started.store(index + 1U, std::memory_order_release);
            EXPECT_TRUE(unit_.UpdateKnownRegion(reg.GetStartAddress(), reg.GetEndAddress()));
            insert_finished.store(index + 1U, std::memory_order_release);
            std::this_thread::sleep_for(2ms);
        }

        arrive_and_wait();

        for (std::size_t index = 0U; index < memory_regions.size(); ++index)
        {
            const auto& reg = memory_regions[index];
            remove_started.store(index + 1U, std::memory_order_release);
            unit_.RemoveKnownRegion(reg.GetStartAddress());
            remove_finished.store(index + 1U, std::memory_order_release);
            std::this_thread::sleep_for(2ms);
        }
    };

    auto reader_activity = [&]() {
        std::random_device rd;   // Will be used to obtain a seed for the random number engine
        std::mt19937 gen(rd());  // Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<std::size_t> distrib(0U, memory_regions.size() - 1U);

        // Phase 1: regions are being inserted in index order - check lookups against insertion progress.
        for (std::uint8_t i = 0U; i < 100U; i++)
        {
            const auto index = distrib(gen);
            const auto& region = memory_regions[index];

            const auto finished_before = insert_finished.load(std::memory_order_acquire);
            const auto bounds = unit_.GetBoundsFromAddress(region.GetStartAddress());
            const auto started_after = insert_started.load(std::memory_order_acquire);

            if (index < finished_before)
            {
                // insertion was already complete before we even looked - must be found
                EXPECT_TRUE(bounds.has_value());
                if (bounds.has_value())
                {
                    EXPECT_EQ(*bounds, region);
                }
            }
            else if (index >= started_after)
            {
                // insertion hadn't even started, even after we looked - must not be found
                EXPECT_FALSE(bounds.has_value());
            }
            else if (bounds.has_value())
            {
                // insertion overlapped with our lookup - no expectation, but if found it must be correct
                EXPECT_EQ(*bounds, region);
            }
            std::this_thread::sleep_for(4ms);
        }

        arrive_and_wait();

        // Phase 2: regions are being removed in index order - check lookups against removal progress.
        for (std::uint8_t i = 0U; i < 100U; i++)
        {
            const auto index = distrib(gen);
            const auto& region = memory_regions[index];

            const auto finished_before = remove_finished.load(std::memory_order_acquire);
            const auto bounds = unit_.GetBoundsFromAddress(region.GetStartAddress());
            const auto started_after = remove_started.load(std::memory_order_acquire);

            if (index < finished_before)
            {
                // removal was already complete before we even looked - must not be found
                EXPECT_FALSE(bounds.has_value());
            }
            else if (index >= started_after)
            {
                // removal hadn't even started, even after we looked - must still be found
                EXPECT_TRUE(bounds.has_value());
                if (bounds.has_value())
                {
                    EXPECT_EQ(*bounds, region);
                }
            }
            else if (bounds.has_value())
            {
                // removal overlapped with our lookup - no expectation, but if found it must be correct
                EXPECT_EQ(*bounds, region);
            }
            std::this_thread::sleep_for(4ms);
        }
    };

    std::thread writer{writer_activity};

    std::thread reader_1{reader_activity};
    std::thread reader_2{reader_activity};
    std::thread reader_3{reader_activity};
    std::thread reader_4{reader_activity};

    writer.join();
    reader_1.join();
    reader_2.join();
    reader_3.join();
    reader_4.join();
}

TEST_F(MockMemoryRegionMapTest, ExceedingMaxCallsToAcquiringVersionForReadingWhileWritingReturnsBlank)
{
    // Given that the operation to update the chosen version's ref count to indicate that it is being currently
    // written to fails every time
    ExpectAcquireRegionVersionForOverwriteCannotAcquireRegion();

    // When trying to acquire a version for writing
    const auto index = attorney_.AcquireRegionVersionForOverwrite();

    // Then we can't acquire a version
    EXPECT_FALSE(index.has_value());
}

TEST_F(MockMemoryRegionMapTest, AcquiringVersionForWritingWhenAllVersionsAreCurrentlyBeingReadReturnsBlank)
{
    // Given that the operation to update the chosen version's ref count to indicate that it is being currently
    // read returns a value indicating that it is already being written every time,
    ExpectAcquireRegionVersionForReadCannotAcquireRegion();

    // When trying to acquire a version for reading
    const auto index = attorney_.AcquireLatestRegionVersionForRead();

    // Then we can't acquire a version
    EXPECT_FALSE(index.has_value());
}

using MemoryRegionMapDeathTest = MemoryRegionMapTest;
TEST_F(MemoryRegionMapDeathTest, RemovingNonExistantRegionTerminates)
{
    const uint8_t start_address{50};
    const MemoryRegionBounds memoryBounds{start_address, 100U};

    // Given a memory region is inserted into the MemoryRegionMap
    EXPECT_TRUE(unit_.UpdateKnownRegion(memoryBounds.GetStartAddress(), memoryBounds.GetEndAddress()));

    // When removing a memory range that hasn't been inserted
    // Then the program terminates
    EXPECT_DEATH(unit_.RemoveKnownRegion(start_address + 1), ".*");
}

class MockMemoryRegionMapDeathTest : public MockMemoryRegionMapTest
{
  protected:
    const bool is_death_test_{true};
};

TEST_F(MockMemoryRegionMapDeathTest, ExceedingMaxConcurrentReadersWhenAcquiringVersionForReadingTerminates)
{
    // When the ref count of a slot is equal to INVALID_REF_COUNT_INTERVAL_START - 1U, indicating that the number of
    // concurrent readers has caused the ref count to overflow into the invalid ref count range
    EXPECT_CALL(atomic_mock_, fetch_add(_, _))
        .Times(::testing::AtMost(1U))
        .WillRepeatedly(Return(MemoryRegionMapAttorney::INVALID_REF_COUNT_INTERVAL_START - 1U));

    // When trying to acquire a version for reading
    // Then the program terminates
    EXPECT_DEATH(attorney_.AcquireLatestRegionVersionForRead(), ".*");
}

TEST_F(MockMemoryRegionMapDeathTest, ExceedingMaxConcurrentReadersWhenAcquiringVersionForReadingDuringWritingTerminates)
{
    // When the ref count of a slot is equal to INVALID_REF_COUNT_INTERVAL_END, indicating that the number of
    // concurrent readers trying to read a version while it's being written to has caused the ref count to overflow back
    // to a ref count of 0
    EXPECT_CALL(atomic_mock_, fetch_add(_, _))
        .Times(::testing::AtMost(1U))
        .WillRepeatedly(Return(MemoryRegionMapAttorney::INVALID_REF_COUNT_INTERVAL_END));

    // When trying to acquire a version for reading
    // Then the program terminates
    EXPECT_DEATH(attorney_.AcquireLatestRegionVersionForRead(), ".*");
}

TEST_F(MockMemoryRegionMapDeathTest, FailingToAcquireWriteVersionWhenUpdatingRegionTerminates)
{
    // Given that the operation to update the chosen version's ref count to indicate that it is being currently
    // written to fails every time
    ExpectAcquireRegionVersionForOverwriteCannotAcquireRegion(is_death_test_);

    // When trying to update a known region
    // Then the program terminates
    EXPECT_DEATH(unit_.UpdateKnownRegion(std::uintptr_t{50U}, std::uintptr_t{100}), ".*");
}

TEST_F(MockMemoryRegionMapDeathTest, FailingToAcquireWriteVersionWhenRemovingRegionTerminates)
{
    // Given that the operation to update the chosen version's ref count to indicate that it is being currently
    // written to fails every time
    ExpectAcquireRegionVersionForOverwriteCannotAcquireRegion(is_death_test_);

    // When trying to remove a known region
    // Then the program terminates
    EXPECT_DEATH(unit_.RemoveKnownRegion(std::uintptr_t{50U}), ".*");
}

TEST_F(MockMemoryRegionMapDeathTest, FailingToAcquireWriteVersionWhenClearingRegionTerminates)
{
    // Given that the operation to update the chosen version's ref count to indicate that it is being currently
    // written to fails every time
    ExpectAcquireRegionVersionForOverwriteCannotAcquireRegion(is_death_test_);

    // When trying to remove a clear all known regions
    // Then the program terminates
    EXPECT_DEATH(unit_.ClearKnownRegions(), ".*");
}

TEST_F(MockMemoryRegionMapDeathTest, FailingToAcquireReadVersionWhenGettingBoundsTerminates)
{
    // Given that the operation to update the chosen version's ref count to indicate that it is being currently
    // read returns a value indicating that it is already being written every time,
    ExpectAcquireRegionVersionForReadCannotAcquireRegion(is_death_test_);

    // When trying to get bounds for a memory address
    // Then the program terminates
    EXPECT_DEATH(unit_.GetBoundsFromAddress(std::uintptr_t{50U}), ".*");
}

TEST_F(MockMemoryRegionMapDeathTest, FailingToAcquireReadVersionWhenGettingSizeTerminates)
{
    // Given that the operation to update the chosen version's ref count to indicate that it is being currently
    // read returns a value indicating that it is already being written every time,
    ExpectAcquireRegionVersionForReadCannotAcquireRegion(is_death_test_);

    // When trying to get the size of a version
    // Then the program terminates
    EXPECT_DEATH(unit_.GetSize(), ".*");
}

}  // namespace score::memory::shared::test
