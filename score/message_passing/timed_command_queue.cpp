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
#include "score/message_passing/timed_command_queue.h"

#include <score/utility.hpp>

namespace score
{
namespace message_passing
{
namespace detail
{

void TimedCommandQueue::RegisterImmediateEntry(Entry& entry, QueuedCallback callback, const void* const owner) noexcept
{
    RegisterTimedEntry(entry, TimePoint{}, std::move(callback), owner);
}

void TimedCommandQueue::RegisterTimedEntry(Entry& entry,
                                           const TimePoint until,
                                           QueuedCallback callback,
                                           const void* const owner) noexcept
{
    entry.until_ = until;
    entry.owner_ = owner;
    entry.callback_ = std::move(callback);

    std::lock_guard<std::mutex> guard(mutex_);
    score::cpp::ignore = queue_.insert(std::find_if(queue_.begin(),
                                             queue_.end(),
                                             [until](const Entry& queue_entry) {
                                                 return until < queue_entry.until_;
                                             }),
                                entry);
}

TimedCommandQueue::TimePoint TimedCommandQueue::ProcessQueue(const TimePoint now) noexcept
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!queue_.empty())
    {
        Entry& queue_entry = queue_.front();
        const TimePoint until = queue_entry.until_;
        if (until > now)
        {
            return until;
        }
        queue_.pop_front();
        {
            QueuedCallback callback = std::move(queue_entry.callback_);
            lock.unlock();  // give the callback ability to reschedule itself
            callback(now);
        }
        lock.lock();
    }
    return TimePoint{};
}

void TimedCommandQueue::CleanUpOwner(const void* const owner) noexcept
{
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.remove_and_dispose_if(
        [owner](auto& queue_entry) {
            return queue_entry.owner_ == owner;
        },
        [&lock](auto queue_entry) noexcept {
            lock.unlock();
            queue_entry->callback_ = QueuedCallback{};
            lock.lock();
        });
}

}  // namespace detail
}  // namespace message_passing
}  // namespace score
