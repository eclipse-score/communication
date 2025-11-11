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
#ifndef SCORE_LIB_MESSAGE_PASSING_TIMED_COMMAND_QUEUE_H
#define SCORE_LIB_MESSAGE_PASSING_TIMED_COMMAND_QUEUE_H

#include "score/message_passing/timed_command_queue_entry.h"

#include <mutex>

namespace score
{
namespace message_passing
{
namespace detail
{

/// \brief Intrusive list based priority queue ordered by a time point
/// \details The TimedCommandQueue is designed to queue commands for serialized immediate or delayed execution
///          and to execute them in a sequential order. The queue uses an ordered intrusive linked list with
///          TimedCommandQueueEntry objects as list elements. The interface is thread-safe: the queue integrity is
///          protected by a mutex.
class TimedCommandQueue
{
  public:
    using Entry = TimedCommandQueueEntry;
    using Clock = TimedCommandQueueEntry::Clock;
    using TimePoint = TimedCommandQueueEntry::TimePoint;
    using QueuedCallback = TimedCommandQueueEntry::QueuedCallback;

    /// \brief Inserts a queue entry for "immediate" execution
    /// \details The entry will be placed into the queue after all other entries for "immediate" execution, but before
    ///          any timed entry.
    /// \param entry intrusive list entry to use in the queue
    /// \param callback callback to execute when the entry is processed
    /// \param owner an optional key to use in CleanUpOwner()
    void RegisterImmediateEntry(Entry& entry, QueuedCallback callback, const void* const owner = nullptr) noexcept;

    /// \brief Inserts a queue entry for delayed execution
    /// \details The entry will be placed into the queue after all entries for "immediate" execution, after all entries
    ///          with the same or earlier execution time point, but before any entry with later execution time point
    /// \param entry intrusive list entry to use in the queue
    /// \param until time point at which to execute the callback
    /// \param callback callback to execute when the entry is processed
    /// \param owner an optional key to use in CleanUpOwner()
    void RegisterTimedEntry(Entry& entry,
                            const TimePoint until,
                            QueuedCallback callback,
                            const void* const owner = nullptr) noexcept;

    /// \brief Process the queue till the given time point
    /// \details Sequentially processes all the "immediate" entries and all the timed entries up to the specified
    ///          time point. A processed entry is first removed from the list, then its callback is moved to a local
    ///          variable and called, then the callback local variable is destructed (which invokes destructors for the
    ///          callback capture). This sequence allows to re-queue the same entry back into the queue while being
    ///          processed, if needed.
    /// \param now time point till which to process the queue
    /// \return the time point of the first entry still in the queue; TimePoint{} if the queue is empty
    TimePoint ProcessQueue(const TimePoint now) noexcept;

    /// \brief Removes the queue entries owned by a particular entity
    /// \details Provides a bulk clean up of the part of the queue associated with a particular user of a shared queue.
    ///          This gives the user an ability to end the lifetime of the resources associated with all their currently
    ///          queued entries without waiting for their execution. During the cleanup, the callbacks of the removed
    ///          entries are not called but destructed, which invokes destructors of their captured values.
    /// \param owner the owner argument of Register...() calls for the entries to remove. The value shall normally be
    ///              the address of an object responsible for the lifetimes of the queue entry objects. To avoid
    ///              unexpected interference between multiple users of a shared queue, the nullptr value, if provided,
    ///              does not cause any entry to be removed.
    void CleanUpOwner(const void* const owner) noexcept;

  private:
    std::mutex mutex_;
    score::containers::intrusive_list<Entry, TimedCommandQueue> queue_;
};

}  // namespace detail
}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_TIMED_COMMAND_QUEUE_H
