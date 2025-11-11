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
#include <gtest/gtest.h>

#include "score/message_passing/timed_command_queue.h"

namespace score::message_passing
{
namespace
{

using Queue = detail::TimedCommandQueue;
using Entry = Queue::Entry;
using TimePoint = Queue::TimePoint;

using std::literals::chrono_literals::operator""s;

/// The class to record whether a queue entry callback was called
///
/// The object of this class is moved to the callback capture. If the callback is called, it explicitly
/// abandons the flag pointer. If the flag pointer is not abandoned on the destruction of the callback
/// (such as when it was cleaned up from the queue by CleanUpOwner()), the flag will be set.
class CleanUpEvent
{
  public:
    explicit CleanUpEvent(bool& flag) : flag_ptr_{&flag}
    {
        flag = false;
    }

    CleanUpEvent(CleanUpEvent&& other) noexcept : flag_ptr_{other.flag_ptr_}
    {
        other.Abandon();
    }

    ~CleanUpEvent() noexcept
    {
        if (flag_ptr_ != nullptr)
        {
            *flag_ptr_ = true;
        }
    }

    void Abandon() const noexcept
    {
        flag_ptr_ = nullptr;
    }

    CleanUpEvent(const CleanUpEvent&) = delete;
    CleanUpEvent& operator=(const CleanUpEvent&) = delete;
    CleanUpEvent& operator=(CleanUpEvent&&) = delete;

  private:
    mutable bool* flag_ptr_;
};

TEST(TimedCommandQueueTest, EmptyQueue)
{
    Queue queue{};
    EXPECT_EQ(queue.ProcessQueue(TimePoint{}), TimePoint{});
}

TEST(TimedCommandQueueTest, ImmediateEntry)
{
    Queue queue{};
    Entry immediate_entry{};
    queue.RegisterImmediateEntry(immediate_entry, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{});
    });
    EXPECT_EQ(queue.ProcessQueue(TimePoint{}), TimePoint{});

    queue.RegisterImmediateEntry(immediate_entry, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{1s});
    });
    EXPECT_EQ(queue.ProcessQueue(TimePoint{1s}), TimePoint{});
}

TEST(TimedCommandQueueTest, TimedEntry)
{
    Queue queue{};
    Entry timed_entry1s{};
    Entry timed_entry2s{};
    queue.RegisterTimedEntry(timed_entry1s, TimePoint{1s}, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{1s});  // scheduled at exact time
    });
    queue.RegisterTimedEntry(timed_entry2s, TimePoint{2s}, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{3s});  // scheduled with delay
    });
    EXPECT_EQ(queue.ProcessQueue(TimePoint{}), TimePoint{1s});
    EXPECT_EQ(queue.ProcessQueue(TimePoint{1s}), TimePoint{2s});  // schedule at exact time
    EXPECT_EQ(queue.ProcessQueue(TimePoint{3s}), TimePoint{});    // schedule with delay
}

TEST(TimedCommandQueueTest, QueueSorting)
{
    Queue queue{};
    Entry immediate_entry1{};
    Entry immediate_entry2{};
    Entry immediate_entry3{};
    Entry timed_entry1s{};
    Entry timed_entry2s{};
    Entry timed_entry3s{};
    queue.RegisterImmediateEntry(immediate_entry1, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{1s});
    });
    queue.RegisterTimedEntry(timed_entry1s, TimePoint{1s}, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{1s});
    });
    queue.RegisterImmediateEntry(immediate_entry2, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{1s});
    });
    queue.RegisterTimedEntry(timed_entry3s, TimePoint{3s}, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{3s});
    });
    queue.RegisterImmediateEntry(immediate_entry3, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{1s});
    });
    queue.RegisterTimedEntry(timed_entry2s, TimePoint{2s}, [](TimePoint time) {
        EXPECT_EQ(time, TimePoint{2s});
    });
    EXPECT_EQ(queue.ProcessQueue(TimePoint{1s}), TimePoint{2s});
    EXPECT_EQ(queue.ProcessQueue(TimePoint{2s}), TimePoint{3s});
    EXPECT_EQ(queue.ProcessQueue(TimePoint{3s}), TimePoint{});
}

TEST(TimedCommandQueueTest, CleanUpOwner)
{
    Queue queue{};
    bool owner1{};
    bool owner2{};
    bool owner3{};
    CleanUpEvent event1{owner1};
    CleanUpEvent event2{owner2};
    CleanUpEvent event3{owner3};
    Entry immediate_entry1{};
    Entry immediate_entry2{};
    Entry immediate_entry3{};
    queue.RegisterImmediateEntry(
        immediate_entry1,
        [event = std::move(event1)](TimePoint) noexcept {
            event.Abandon();
        },
        &owner1);
    queue.RegisterImmediateEntry(
        immediate_entry2,
        [event = std::move(event2)](TimePoint) noexcept {
            event.Abandon();
        },
        &owner2);
    queue.RegisterImmediateEntry(
        immediate_entry3,
        [event = std::move(event3)](TimePoint) noexcept {
            event.Abandon();
        },
        &owner3);
    queue.CleanUpOwner(&owner1);
    queue.CleanUpOwner(&owner2);
    EXPECT_EQ(queue.ProcessQueue(TimePoint{}), TimePoint{});

    EXPECT_TRUE(owner1);
    EXPECT_TRUE(owner2);
    EXPECT_FALSE(owner3);
}

}  // namespace
}  // namespace score::message_passing
