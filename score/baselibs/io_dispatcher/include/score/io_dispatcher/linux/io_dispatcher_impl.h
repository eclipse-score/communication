/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

/********************************************************************************
 * Linux backend of the io_dispatcher.
 *
 * Demultiplexes IO readiness with poll(2) (score::os::SysPoll) and is unblocked via a
 * self-pipe (score::os::Unistd::pipe) whose read end is always part of the poll set.
 * poll is level-triggered: a source stays ready until it is drained.
 * Because poll takes a fresh fd array per call, registration changes
 * unblock a blocked HandleEvents() so the new poll set takes effect promptly.
 ********************************************************************************/

#ifndef SCORE_IO_DISPATCHER_LINUX_IO_DISPATCHER_IMPL_H
#define SCORE_IO_DISPATCHER_LINUX_IO_DISPATCHER_IMPL_H

#include "score/containers/non_relocatable_vector.h"
#include "score/io_dispatcher/io_dispatcher.h"
#include "score/result/result.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace io_dispatcher
{

namespace internal
{
/**
 * @brief Definition of monitored event source.
 */
struct EventSourceEntry
{
    EventSourceEntry() = default;

    EventSourceEntry(const EventSourceEntry&) = delete;
    EventSourceEntry& operator=(const EventSourceEntry&) = delete;
    EventSourceEntry(EventSourceEntry&&) = delete;
    EventSourceEntry& operator=(EventSourceEntry&&) = delete;
    ~EventSourceEntry() = default;

    /**
     * @brief Native handle (fd) the callback is registered for.
     */
    NativeHandle io_source_{kInvalidNativeHandle};

    /**
     * @brief Event types the callback is currently active for.
     */
    EventTypes registered_events_{};

    /**
     * @brief The registered callback function object.
     */
    CallbackType callback_{};

    /**
     * @brief True while the slot holds a registered callback.
     */
    bool valid_{false};

    /**
     * @brief True while the callback is currently executing.
     */
    bool in_callback_{false};

    /**
     * @brief Thread running the callback, meaningful only while `in_callback_` is set.
     */
    std::thread::id callback_thread_{};

    /**
     * @brief Bumped on every (re)registration into this slot; part of the EventSourceHandle.
     */
    std::uint64_t sequence_num_{0U};

    /**
     * @brief Protects this entry.
     */
    mutable std::mutex mutex_{};
};
}  // namespace internal

/**
 * @brief Linux implementation of the reactor (poll + self-pipe).
 */
class IoDispatcherImpl final : public IoDispatcher
{
  public:
    /**
     * @brief Acquire the resources and create the IO dispatcher.
     * @param max_num_event_sources Max number of callbacks, valid range 0..8191.
     * @param tag Stamped into every handle this dispatcher issues.
     * @error kApiError tag exceeds kMaxDispatcherTag.
     * @error kResource max_num_event_sources out of range or a system resource could not be acquired.
     */
    static score::Result<std::unique_ptr<IoDispatcher>> Create(const std::uint16_t max_num_event_sources,
                                                               const DispatcherTag tag = 0U) noexcept;

    ~IoDispatcherImpl() noexcept override;

    score::Result<EventSourceHandle> Register(const NativeHandle io_source,
                                              const EventTypes events,
                                              CallbackType callback) noexcept override;
    score::Result<void> SetMonitoredEvents(const EventSourceHandle event_source_handle,
                                           const EventTypes events) noexcept override;
    /**
     * @brief Replace the callback target. The callback must not be currently executing.
     */
    score::Result<void> SetCallbackTarget(const EventSourceHandle event_source_handle,
                                          CallbackType callback) noexcept override;
    /**
     * @brief Unregister a callback.
     * @note Waits for a running callback of this source, so it does not run after this call returns.
     *       Called from within that callback, it returns at once.
     * @note From another thread, also waits for a blocked poll() to return, so closing the source frees it.
     * @warning Deadlocks when the caller holds something the running callback of this source waits for.
     */
    void Unregister(const EventSourceHandle event_source_handle) noexcept override;
    score::Result<UnblockReason> HandleEvents(const std::chrono::milliseconds timeout) noexcept override;
    void Unblock() noexcept override;

  private:
    IoDispatcherImpl(const std::uint16_t max_num_event_sources,
                     const DispatcherTag tag,
                     const NativeHandle unblock_read_fd,
                     const NativeHandle unblock_write_fd) noexcept;

    /**
     * @brief Find a free slot for io_source; errors if io_source is already registered
     *        (kApiError) or no slot is free (kResource). Caller holds wait_mutex_.
     */
    score::Result<std::size_t> FindFreeSlot(const NativeHandle io_source) noexcept;

    /**
     * @brief Runs `accessor` under wait_mutex_, while the dispatch thread is outside poll().
     * @note Wakes a poll() in progress first.
     * @note The dispatch thread builds no new poll set while an access is pending.
     */
    template <typename Accessor>
    auto AccessSources(Accessor&& accessor) noexcept -> decltype(accessor());

    /**
     * @brief Dispatch a single ready callback (locks the entry only around bookkeeping).
     */
    void HandleOneEvent(const EventSourceHandle event_source_handle, const EventTypes events) noexcept;

    /**
     * @brief Drain the self-pipe after an unblock.
     */
    void HandleUnblock() noexcept;

    DispatcherTag tag_;
    score::containers::NonRelocatableVector<internal::EventSourceEntry> callbacks_;
    NativeHandle unblock_read_fd_;
    NativeHandle unblock_write_fd_;
    /// Held by HandleEvents() from building its poll set until poll() returns, and by every access to the sources.
    std::mutex wait_mutex_;
    /// Notified when the last pending access releases wait_mutex_.
    std::condition_variable accesses_finished_;
    /// Accesses that want wait_mutex_; HandleEvents() starts no poll() while any is pending.
    std::atomic<std::size_t> num_of_accesses_{0U};
    /// True from HandleEvents() taking wait_mutex_ until poll() returns, so an access wakes the poll().
    std::atomic<bool> in_wait_{false};
};

}  // namespace io_dispatcher

#endif  // SCORE_IO_DISPATCHER_LINUX_IO_DISPATCHER_IMPL_H
