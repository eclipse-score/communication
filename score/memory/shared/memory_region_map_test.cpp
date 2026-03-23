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
#include "score/memory/shared/atomic_indirector.h"
#include "score/memory/shared/atomic_mock.h"

#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <random>
#include <thread>
#include <vector>

namespace score::memory::shared::test
{

using ::testing::_;
using ::testing::Return;

class MemoryRegionMapAttorney
{
  public:
    using MemoryRegionMapMock = detail::MemoryRegionMapImpl<AtomicIndirectorMock>;
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
    using MemoryRegionMapMock = detail::MemoryRegionMapImpl<AtomicIndirectorMock>;
    using AtomicType = MemoryRegionMapAttorney::RegionVersionRefCountType;

    MockMemoryRegionMapTest() : unit_{}, attorney_{unit_}, atomic_mock_{}
    {
        AtomicIndirectorMock<AtomicType>::SetMockObject(&atomic_mock_);

        // We set the ref count of all versions to 0 because the version acquisition algorithm treats unused versions
        // (i.e. versions which still have the default initial ref count value) differently.
        attorney_.SetAllSlotRefCountsToZero();
    }

    ~MockMemoryRegionMapTest()
    {
        AtomicIndirectorMock<AtomicType>::SetMockObject(nullptr);
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
    AtomicMock<AtomicType> atomic_mock_;
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
/// \details We have 100 test regions, which are step-by-step inserted into the region map and then step-by-step
///          removed again by the writer, which sleeps between each region map change. After the writer has done
///          an insert or remove, he notes down the current state of this region (inserted = true/false).
///          The readers all do 100 times randomly take one out of the test regions and do a bounds-lookup of this
///          test regions start-address. They then either get a positive result (region found) or a negative/nullptr
///          (region not found). In both cases the readers do a plausibility check:
///          If the region was found - either directly before or after the bounds-check the inserted flag noted by the
///          writer needed to be true (runtime of the writer and sleep times of the readers are such, that it is almost
///          impossible, that the flag is false at both times, but the reader still gets a positive bounds-check!)
///          If the region was NOT found - either directly before or after the bounds-check the inserted flag noted by
///          the writer needed to be false (runtime of the writer and sleep times of the readers are such, that it is
///          almost impossible, that the flag is true at both times, but the reader still gets a negative bounds-check!)
TEST_F(MemoryRegionMapTest, ConcurrentAccess)
{
    struct RegionWithFlag
    {
        RegionWithFlag(MemoryRegionBounds region, bool insertedFlag) noexcept
            : region_(region), inserted_(insertedFlag) {};

        RegionWithFlag(RegionWithFlag&& other) noexcept
        {
            std::swap(region_, other.region_);
            inserted_.store(other.inserted_);
        };
        MemoryRegionBounds region_;
        std::atomic_bool inserted_;
    };
    using namespace std::chrono_literals;

    // given 100 memory regions
    std::vector<RegionWithFlag> memory_regions;
    memory_regions.reserve(100);
    // each with a size of 50 bytes
    constexpr std::uint8_t MEM_REGION_SIZE{50};

    for (unsigned int i = 0; i < 100; i++)
    {
        memory_regions.emplace_back(
            MemoryRegionBounds{static_cast<uintptr_t>(i * 100 + 1U), static_cast<uintptr_t>(i * 100 + MEM_REGION_SIZE)},
            false);
    }

    // and one writer thread, which first inserts and afterwards removes these memory regions
    auto writer_activity = [&memory_regions, this]() {
        for (auto& reg : memory_regions)
        {
            EXPECT_TRUE(unit_.UpdateKnownRegion(reg.region_.GetStartAddress(), reg.region_.GetEndAddress()));
            reg.inserted_.store(true, std::memory_order_seq_cst);
            std::this_thread::sleep_for(2ms);
        }

        for (auto& reg : memory_regions)
        {
            // we have inserted it in the 1st run ... let's be hyper-cautious
            ASSERT_TRUE(reg.inserted_);
            unit_.RemoveKnownRegion(reg.region_.GetStartAddress());
            reg.inserted_.store(false, std::memory_order_seq_cst);
            std::this_thread::sleep_for(2ms);
        }
    };

    auto reader_activity = [&memory_regions, this]() {
        std::random_device rd;   // Will be used to obtain a seed for the random number engine
        std::mt19937 gen(rd());  // Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> distrib(0, 99);

        for (std::uint8_t i = 0; i < 100; i++)
        {
            const auto random_index = distrib(gen);
            const auto& region = memory_regions[static_cast<std::size_t>(random_index)];
            const bool inserted_before = region.inserted_.load(std::memory_order_seq_cst);
            const auto bounds = unit_.GetBoundsFromAddress(region.region_.GetStartAddress());
            const bool inserted_after = region.inserted_.load(std::memory_order_seq_cst);

            if (bounds.has_value())
            {
                // if map contains region ...
                // expect that inserted flag directly before or after the lookup was true
                EXPECT_TRUE(inserted_before || inserted_after);
                EXPECT_EQ(*bounds, region.region_);
            }
            else
            {
                // if map doesn't contain region ...
                // expect that inserted flag wasn't true before and after the lookup
                EXPECT_FALSE(inserted_before && inserted_after);
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
