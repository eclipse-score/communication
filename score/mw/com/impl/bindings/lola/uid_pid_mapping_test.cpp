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
#include "score/mw/com/impl/bindings/lola/uid_pid_mapping.h"

#include "score/memory/shared/atomic_indirector.h"
#include "score/memory/shared/atomic_mock.h"

#include <gtest/gtest.h>

#include <thread>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr std::uint16_t kMaxNumberOfMappings{10};
using Allocator = std::allocator<lola::UidPidMappingEntry>;
using testing::_;
using testing::Return;

TEST(UidPidMapping, RegisterUpToMaxNumberMappingsSucceeds)
{
    // Given a UidPidMapping with a max number of supported mappings of kMaxNumberOfMappings
    UidPidMapping<Allocator> unit{kMaxNumberOfMappings, Allocator()};
    // when inserting kMaxNumberOfMappings different PIDs
    for (std::uint16_t i = 0; i < kMaxNumberOfMappings; i++)
    {
        auto result = unit.RegisterPid(i, 100 + i);
        // expect, that registering was successful
        EXPECT_TRUE(result.has_value());
        // and that the registered PID is returned
        EXPECT_EQ(result.value(), 100 + i);
    }
    // and when registering another uid beyond the capacity
    auto result = unit.RegisterPid(42, 142);
    // expect, that the result is empty.
    EXPECT_FALSE(result.has_value());
}

TEST(UidPidMapping, RegisterMaxRetryFailure)
{
    // Given kMaxNumberOfMappings, which are all but one "kUsed" for uid 0 .. kMaxNumberOfMappings - 1
    score::containers::DynamicArray<UidPidMappingEntry> mapping_entries(kMaxNumberOfMappings);

    const std::uint16_t unused_entry{5};

    for (std::uint16_t i = 0; i < kMaxNumberOfMappings; i++)
    {
        if (i != unused_entry)
        {
            mapping_entries.at(i).SetStatusAndUidAtomic(UidPidMappingEntry::MappingEntryStatus::kUsed, i);
            mapping_entries.at(i).pid_ = i;
        }
        else
        {
            mapping_entries.at(i).SetStatusAndUidAtomic(UidPidMappingEntry::MappingEntryStatus::kUnused, i);
            mapping_entries.at(i).pid_ = 0;
        }
    }

    memory::shared::AtomicMock<UidPidMappingEntry::key_type> atomic_mock{};
    memory::shared::AtomicIndirectorMock<UidPidMappingEntry::key_type>::SetMockObject(&atomic_mock);

    const std::size_t kMaxRetries{50};
    // Given the operation to update the mapping entry fails kMaxRetries times
    EXPECT_CALL(atomic_mock, compare_exchange_weak(_, _, _)).Times(kMaxRetries).WillRepeatedly(Return(false));

    // when trying to register a new uid 42 (not among the registered uids)
    auto result = lola::detail::RegisterPid<memory::shared::AtomicIndirectorMock>(
        mapping_entries.begin(), mapping_entries.end(), 42, 142);
    // expect, that an empty optional is returned.
    EXPECT_FALSE(result.has_value());
}

TEST(UidPidMapping, ReregisterPIDSuccess)
{
    // Given a UidPidMapping with a max number of supported mappings of kMaxNumberOfMappings
    UidPidMapping<Allocator> unit{kMaxNumberOfMappings, Allocator()};
    // when registering a PID for a given uid
    auto result = unit.RegisterPid(42, 142);
    // expect, that registering was successful
    EXPECT_TRUE(result.has_value());
    // and that the registered PID is returned
    EXPECT_EQ(result.value(), 142);
    // and when registering another PID for the same uid
    result = unit.RegisterPid(42, 999);
    // expect, that the result is not empty.
    EXPECT_TRUE(result.has_value());
    // and that the previously registered PID is returned
    EXPECT_EQ(result.value(), 142);
}

TEST(UidPidMapping, RegisterUpdatesPidWhenEntryIsInUpdatingStateForSameUid)
{
    // Given a single entry array that can be controlled
    score::containers::DynamicArray<UidPidMappingEntry> mapping_entries(1);

    const uid_t test_uid = 42;
    const pid_t new_pid = 200;
    const pid_t old_pid = 100;
    // Set up the entry in kUpdating state for our test UID
    mapping_entries.at(0).SetStatusAndUidAtomic(UidPidMappingEntry::MappingEntryStatus::kUpdating, test_uid);
    mapping_entries.at(0).pid_ = old_pid;

    // When tried to register
    auto result = lola::detail::RegisterPid(mapping_entries.begin(), mapping_entries.end(), test_uid, new_pid);

    // Then the operation should succeed and return the new PID
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), new_pid);
}

TEST(UidPidMapping, ConcurrentAccess)
{
    // Given a UidPidMapping with a max number of supported mappings of 100
    UidPidMapping<Allocator> unit{100, Allocator()};
    using namespace std::chrono_literals;

    // given a thread, which tries to Register 30 different uid/pid pairs, by ...
    auto thread_action = [&unit](uid_t start_uid) {
        for (int i = 0; i < 30; i++)
        {
            uid_t uid = start_uid;
            auto pid = static_cast<pid_t>(uid + 100);
            for (int j = 0; j < 3; j++)
            {
                // ... calling Register for a specific/unique uid a pid
                auto result = unit.RegisterPid(uid, pid);
                // and expects, that register was successful
                EXPECT_TRUE(result.has_value());
                // and returns the newly registered pid
                EXPECT_EQ(result.value(), pid);
                uid++;
                pid++;
            }
            // and then sleeps
            std::this_thread::sleep_for(50ms);
        }
    };

    // we start 3 of those threads, which will in sum register 90 uid/pid pairs, which needs to fit in the 100 capacity.
    std::thread t1(thread_action, 100);
    std::thread t2(thread_action, 200);
    std::thread t3(thread_action, 300);
    t1.join();
    t2.join();
    t3.join();
}

TEST(DetailRegisterPidEmpty, NoNullPointerDereferenceInCaseOfEmptyMappingEntriesArray)
{
    constexpr auto number_of_elements{0U};
    score::containers::DynamicArray<UidPidMappingEntry> empty{number_of_elements};
    constexpr std::uint16_t uid{0U};
    constexpr std::uint16_t pid{1U};

    EXPECT_EQ(empty.size(), 0);
    auto result = detail::RegisterPid(empty.begin(), empty.end(), uid, pid);

    EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
