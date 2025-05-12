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
#include "score/mw/com/impl/sample_reference_tracker.h"

#include <gtest/gtest.h>

#include <random>
#include <thread>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{

namespace
{

TEST(SampleReferenceTrackerTest, AllocateAndFreeSamples)
{
    SampleReferenceTracker tracker{3U};
    EXPECT_FALSE(tracker.IsUsed());
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 3U);

    TrackerGuardFactory guard_factory = tracker.Allocate(2U);
    EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 2U);
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 1U);
    EXPECT_TRUE(tracker.IsUsed());

    std::optional<SampleReferenceGuard> guard1 = guard_factory.TakeGuard();
    ASSERT_TRUE(guard1.has_value());
    EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 1U);
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 1U);
    EXPECT_TRUE(tracker.IsUsed());

    std::optional<SampleReferenceGuard> guard2 = guard_factory.TakeGuard();
    ASSERT_TRUE(guard2.has_value());
    EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 0U);
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 1U);
    EXPECT_TRUE(tracker.IsUsed());

    std::optional<SampleReferenceGuard> guard3 = guard_factory.TakeGuard();
    ASSERT_FALSE(guard3.has_value());
    EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 0U);
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 1U);
    EXPECT_TRUE(tracker.IsUsed());

    guard1 = std::nullopt;
    ASSERT_TRUE(guard2.has_value());
    EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 0U);
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 2U);
    EXPECT_TRUE(tracker.IsUsed());

    guard2 = std::nullopt;
    EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 0U);
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 3U);
    EXPECT_FALSE(tracker.IsUsed());
}

TEST(SampleReferenceTrackerTest, UnusedFactoryRefsAreReturned)
{
    SampleReferenceTracker tracker{3U};
    EXPECT_FALSE(tracker.IsUsed());
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 3U);

    std::optional<SampleReferenceGuard> guard1{};
    {
        TrackerGuardFactory guard_factory = tracker.Allocate(2U);
        EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 2U);
        EXPECT_EQ(tracker.GetNumAvailableSamples(), 1U);
        EXPECT_TRUE(tracker.IsUsed());

        guard1 = guard_factory.TakeGuard();
        ASSERT_TRUE(guard1.has_value());
        EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 1U);
        EXPECT_EQ(tracker.GetNumAvailableSamples(), 1U);
        EXPECT_TRUE(tracker.IsUsed());
    }

    ASSERT_TRUE(guard1.has_value());
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 2U);
    EXPECT_TRUE(tracker.IsUsed());

    guard1 = std::nullopt;
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 3U);
    EXPECT_FALSE(tracker.IsUsed());
}

TEST(SampleReferenceTrackerTest, ChangeSampleNumber)
{
    SampleReferenceTracker tracker{2U};
    EXPECT_FALSE(tracker.IsUsed());
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 2U);

    tracker.Reset(3U);
    EXPECT_FALSE(tracker.IsUsed());
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 3U);

    TrackerGuardFactory guard_factory = tracker.Allocate(3U);
    EXPECT_TRUE(tracker.IsUsed());
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 0U);
    EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 3U);

    std::vector<SampleReferenceGuard> guards{};
    for (std::size_t count = 0U; count < 3U; ++count)
    {
        guards.push_back(std::move(*guard_factory.TakeGuard()));
    }
    EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 0U);

    guards.clear();
    EXPECT_FALSE(tracker.IsUsed());
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 3U);
}

TEST(SampleReferenceTrackerTest, MoveConstructTrackerFactory)
{
    SampleReferenceTracker tracker{3U};
    EXPECT_FALSE(tracker.IsUsed());
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 3U);

    {
        TrackerGuardFactory guard_factory = tracker.Allocate(2U);
        EXPECT_EQ(guard_factory.GetNumAvailableGuards(), 2U);
        EXPECT_EQ(tracker.GetNumAvailableSamples(), 1U);
        EXPECT_TRUE(tracker.IsUsed());

        TrackerGuardFactory moved_factory{std::move(guard_factory)};
        EXPECT_EQ(moved_factory.GetNumAvailableGuards(), 2U);
        EXPECT_EQ(tracker.GetNumAvailableSamples(), 1U);
        EXPECT_TRUE(tracker.IsUsed());

        std::optional<SampleReferenceGuard> guard1 = moved_factory.TakeGuard();
        ASSERT_TRUE(guard1.has_value());
        EXPECT_EQ(moved_factory.GetNumAvailableGuards(), 1U);
        EXPECT_EQ(tracker.GetNumAvailableSamples(), 1U);
        EXPECT_TRUE(tracker.IsUsed());
    }

    EXPECT_FALSE(tracker.IsUsed());
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 3U);
}

TEST(SampleReferenceTrackerTest, ConcurrentlyAcquireSamples)
{
    constexpr static std::size_t NUM_WORKERS = 32U;
    constexpr static std::size_t NUM_SAMPLES = 1337U;
    constexpr static std::size_t NUM_TURNS_PER_WORKER = 999U;

    SampleReferenceTracker tracker{NUM_SAMPLES};

    const auto seed = static_cast<std::default_random_engine::result_type>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    auto thread_func = [&seed, &tracker]() {
        const std::size_t thread_id_num = std::hash<std::thread::id>()(std::this_thread::get_id());

        std::default_random_engine rng{seed + static_cast<std::default_random_engine::result_type>(thread_id_num)};
        std::uniform_int_distribution<std::size_t> sample_allocation{0U, NUM_SAMPLES};

        for (std::size_t turn = 0U; turn < NUM_TURNS_PER_WORKER; ++turn)
        {
            const std::size_t num_samples_to_allocate{sample_allocation(rng)};
            TrackerGuardFactory guard_factory = tracker.Allocate(num_samples_to_allocate);

            const std::size_t guards_allocated{guard_factory.GetNumAvailableGuards()};
            if (guards_allocated > 0U)
            {
                const auto num_guards_to_take{std::uniform_int_distribution<std::size_t>{0U, guards_allocated}(rng)};

                for (std::size_t guard = 0U; guard < num_guards_to_take; ++guard)
                {
                    EXPECT_TRUE(guard_factory.TakeGuard().has_value());
                }
                EXPECT_EQ(guard_factory.GetNumAvailableGuards(), guards_allocated - num_guards_to_take);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    };

    std::vector<std::thread> workers{};
    for (std::size_t t = 0U; t < NUM_WORKERS; ++t)
    {
        workers.emplace_back(thread_func);
    }

    for (auto& thread : workers)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    EXPECT_EQ(tracker.GetNumAvailableSamples(), NUM_SAMPLES);
}

TEST(SampleReferenceTrackerTest, Deallocating)
{
    SampleReferenceTracker tracker{3U};
    EXPECT_EQ(tracker.GetNumAvailableSamples(), 3U);

    TrackerGuardFactory guard_factory = tracker.Allocate(5U);
}

TEST(SampleReferenceTrackerTest, GuardFactoryWillInitiallyHaveNumberOfAvailableGuardsPassedToConstructor)
{
    // Given a SampleReferenceTracker with 3 available samples
    SampleReferenceTracker tracker{3U};

    // and a tracker guard factory with 2 available guards is allocated
    const std::size_t initial_number_available_guards{2U};
    auto tracker_guard_factory = tracker.Allocate(initial_number_available_guards);

    // When getting the number of available guards
    const auto actual_num_available_guards = tracker_guard_factory.GetNumAvailableGuards();

    // Then the number of available guards will be the same number that the factory was initialised with
    EXPECT_EQ(actual_num_available_guards, initial_number_available_guards);
}

TEST(SampleReferenceTrackerTest, TakingAGuardWillDecrementTheNumberOfAvailableGuards)
{
    // Given a SampleReferenceTracker with 3 available samples
    SampleReferenceTracker tracker{3U};

    // and a tracker guard factory with 2 available guards is allocated
    const std::size_t initial_number_available_guards{2U};
    auto tracker_guard_factory = tracker.Allocate(initial_number_available_guards);

    // When taking a guard
    [[maybe_unused]] const auto sample_reference_guard = tracker_guard_factory.TakeGuard();

    // Then the number of available guards will be the number that the factory was initialised with minus 1
    const auto actual_num_available_guards = tracker_guard_factory.GetNumAvailableGuards();
    EXPECT_EQ(actual_num_available_guards, initial_number_available_guards - 1);
}

TEST(SampleReferenceTrackerTest, TakingAGuardWhenThereAreNoSamplesRemainingReturnsEmptyOptional)
{
    // Given a SampleReferenceTracker with 1 available samples
    SampleReferenceTracker tracker{1U};

    // and a tracker guard factory with 1 available guards is allocated
    const std::size_t initial_number_available_guards{1U};
    auto tracker_guard_factory = tracker.Allocate(initial_number_available_guards);

    // and a single guard is taken
    [[maybe_unused]] const auto first_sample_reference_guard = tracker_guard_factory.TakeGuard();

    // When taking a second guard
    const auto second_sample_reference_guard = tracker_guard_factory.TakeGuard();

    // Then an empty optional will be returned
    EXPECT_FALSE(second_sample_reference_guard.has_value());
}

TEST(SampleReferenceTrackerTest, SampleReferenceGuardWillInitiallyHaveNumberOfSamplesPassedToConstructor)
{
    // Given a SampleReferenceTracker with 3 available samples
    const std::size_t initial_num_available_samples{3U};
    SampleReferenceTracker tracker{initial_num_available_samples};

    // When getting the number of available samples
    const auto actual_num_available_samples = tracker.GetNumAvailableSamples();

    // Then the number of available samples will be the same number that the factory was initialised with
    EXPECT_EQ(actual_num_available_samples, initial_num_available_samples);
}

TEST(SampleReferenceTrackerTest, TakingAGuardWillDecrementTheNumberOfSamples)
{
    // Given a SampleReferenceTracker with 3 available samples
    const std::size_t initial_num_available_samples{3U};
    SampleReferenceTracker tracker{initial_num_available_samples};

    // and a tracker guard factory with 2 available guards is allocated
    const std::size_t initial_number_available_guards{2U};
    auto tracker_guard_factory = tracker.Allocate(initial_number_available_guards);

    // When taking a guard
    [[maybe_unused]] const auto sample_reference_guard = tracker_guard_factory.TakeGuard();

    // Then the number of available samples will be the number that the SampleReferenceTracker was initialised with
    // minus the number of guards that were allocated
    const auto actual_num_available_samples = tracker.GetNumAvailableSamples();
    EXPECT_EQ(actual_num_available_samples, initial_num_available_samples - initial_number_available_guards);
}

TEST(SampleReferenceTrackerTest, DestroyingSampleReferenceGuardWillReincrementNumberOfSamples)
{
    // Given a SampleReferenceTracker with 3 available samples
    const std::size_t initial_num_available_samples{3U};
    SampleReferenceTracker tracker{initial_num_available_samples};

    // and a tracker guard factory with 2 available guards is allocated
    const std::size_t initial_number_available_guards{2U};
    auto tracker_guard_factory = tracker.Allocate(initial_number_available_guards);

    // and a guard is taken SampleReferenceGuard
    auto sample_reference_guard = tracker_guard_factory.TakeGuard();

    // When destroying the sample
    sample_reference_guard.reset();

    // Then the number of available samples will be the same number that the factory was initialised with minus the
    // number of guards which are still allocated
    const auto actual_num_available_samples = tracker.GetNumAvailableSamples();
    EXPECT_EQ(actual_num_available_samples, initial_num_available_samples - 1U);
}

TEST(SampleReferenceTrackerTest, SelfMoveAssigningASampleReferenceGuardDoesNotReincrementNumberOfSamples)
{
    // Given a SampleReferenceTracker with 3 available samples
    const std::size_t initial_num_available_samples{3U};
    SampleReferenceTracker tracker{initial_num_available_samples};

    // and a tracker guard factory with 2 available guards is allocated
    const std::size_t initial_number_available_guards{2U};
    auto tracker_guard_factory = tracker.Allocate(initial_number_available_guards);

    // and a guard is taken SampleReferenceGuard
    auto sample_reference_guard = tracker_guard_factory.TakeGuard();

    // When self move assigning the SampleReferenceGuard
    sample_reference_guard = std::move(sample_reference_guard.value());

    // Then the number of available samples will be the number that the SampleReferenceTracker was initialised with
    // minus the number of guards that were allocated
    const auto actual_num_available_samples = tracker.GetNumAvailableSamples();
    EXPECT_EQ(actual_num_available_samples, initial_num_available_samples - initial_number_available_guards);
}

}  // namespace

}  // namespace score::mw::com::impl
