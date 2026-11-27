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
 * QNX backend of the io_dispatcher.
 *
 * A private QNX channel receives pulses.
 * File-descriptor readiness is armed with ionotify() and delivered as pulses on that channel.
 * The wait is bounded with TimerTimeout() and driven by Channel::MsgReceivePulse.
 * Unblock() posts a reserved pulse via Channel::MsgSendPulse.
 * In addition to Read/Write, generic pulses delivered to the channel are surfaced as Pulse events (QNX-only).
 ********************************************************************************/

#ifndef SCORE_IO_DISPATCHER_QNX_IO_DISPATCHER_IMPL_H
#define SCORE_IO_DISPATCHER_QNX_IO_DISPATCHER_IMPL_H

#include "score/io_dispatcher/io_dispatcher.h"
#include "score/result/result.h"
#include <sys/siginfo.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace io_dispatcher
{
namespace internal
{

/**
 * @brief Callback of one registration, shared by the source table and the reactor's dispatch batch.
 */
struct CallbackSlot
{
    static constexpr std::uint8_t kRunning{1U};
    static constexpr std::uint8_t kRetired{2U};

    /**
     * @brief Emptied by `RetireSlot()` once no invocation can start or is still running.
     * @pre Invoked only by the reactor, and only after it sets `kRunning` without finding `kRetired`.
     */
    CallbackType callback{};

    /**
     * @brief `kRunning` while the reactor invokes `callback`, `kRetired` once the registration is gone.
     * @note `kRetired` is never cleared; a new registration or callback target gets a new slot.
     */
    std::atomic<std::uint8_t> flags{0U};
};

/**
 * @brief Definition of monitored event source.
 */
struct EventSourceEntry
{
    /**
     * @brief Monitored descriptor.
     */
    NativeHandle source{kInvalidNativeHandle};

    /**
     * @brief Monitored event types.
     */
    EventTypes events{};

    /**
     * @brief Replaced, never edited in place, so a dispatch batch that copied the old slot still sees it retire.
     */
    std::shared_ptr<CallbackSlot> callback{};

    /**
     * @brief Registered notification event.
     */
    score::cpp::optional<sigevent> notification_event;

    /**
     * @brief True while an arm taken for this file descriptor registration is active.
     *
     * Following case is prevented.
     * Consider a case where two parallel loops are ongoing:
     * - a Register-trigger-Unregister loop
     * - a HandleEvents loop
     * In such case it's possible to rearm after trigger, but before Unregister.
     * Then, an event from previous registration loop can be observed.
     */
    bool armed{false};

    /**
     * @brief Number the pulse of this registration carries next to the descriptor.
     * @note A pulse that the previous socket on a reused descriptor number queued carries another number.
     */
    std::uint32_t generation{0U};
};

/**
 * @brief Received event data.
 */
struct ReceivedEvent
{
    EventSourceHandle handle{kInvalidEventSourceHandle};
    EventTypes events{};
};

using EventContainer = std::vector<ReceivedEvent>;

/**
 * @brief Callback the reactor runs once it releases the source table.
 */
struct DispatchItem
{
    std::shared_ptr<CallbackSlot> slot{};
    EventSourceHandle handle{kInvalidEventSourceHandle};
    EventTypes events{};
};

using DispatchBatch = std::vector<DispatchItem>;

}  // namespace internal

/**
 * @brief QNX implementation of the reactor (channel + pulses + ionotify).
 */
class IoDispatcherImpl final : public IoDispatcher
{
  public:
    /**
     * @brief Acquire the resources and create the IO dispatcher.
     * @param max_num_event_sources Max number of callbacks.
     * @param tag Stamped into every handle this dispatcher issues, pulse handles included.
     * @error kApiError tag exceeds kMaxDispatcherTag.
     * @error kResource max_num_event_sources out of range or a system resource could not be acquired.
     */
    static score::Result<std::unique_ptr<IoDispatcher>> Create(const std::uint16_t max_num_event_sources,
                                                               const DispatcherTag tag = 0U) noexcept;

    ~IoDispatcherImpl() noexcept override;

    score::Result<EventSourceHandle> Register(const NativeHandle io_source,
                                              const EventTypes events,
                                              CallbackType callback) noexcept override;
    /**
     * @brief Register a callback for the pulse code that `events` carries.
     * @error kApiError `events` has no pulse event, its code is reserved by the backend or already registered, or
     *        `callback` is empty.
     * @error kResource no free slot to register another callback.
     * @note Pulses reach the callback when they are sent to GetConnectionId().
     */
    score::Result<EventSourceHandle> RegisterPulse(const EventTypes events, CallbackType callback) noexcept;
    score::Result<void> SetMonitoredEvents(const EventSourceHandle event_source_handle,
                                           const EventTypes events) noexcept override;
    /**
     * @note Waits for a running previous target as `Unregister` does, so it does not run after this call returns.
     * @warning Deadlocks when the caller holds something the running previous target waits for.
     */
    score::Result<void> SetCallbackTarget(const EventSourceHandle event_source_handle,
                                          CallbackType callback) noexcept override;
    /**
     * @brief Unregister a callback.
     * @note Waits for a running callback of this source, so neither the callback nor what it captured outlives
     *       this call.
     *       Called from within that callback, it returns at once and the callback is destroyed when it returns.
     * @warning Deadlocks when the caller holds something the running callback of this source waits for.
     */
    void Unregister(const EventSourceHandle event_source_handle) noexcept override;
    score::Result<UnblockReason> HandleEvents(const std::chrono::milliseconds timeout) noexcept override;
    void Unblock() noexcept override;

    /**
     * @brief Connection id to which generic pulses for pulse callbacks must be sent.
     */
    std::int32_t GetConnectionId() const noexcept
    {
        return connection_id_;
    }

  private:
    enum class PulseResult : std::uint32_t
    {
        kEvent,
        kDuplicateEvent,
        kUnblock,
        kTimeout,
        kSignal
    };

    IoDispatcherImpl(const std::uint16_t max_num_event_sources,
                     const DispatcherTag tag,
                     const std::int32_t channel_id,
                     const std::int32_t connection_id) noexcept;

    score::Result<void> RegisterNotificationEvent(internal::EventSourceEntry& event_source_entry) noexcept;
    void UnregisterNotificationEvent(internal::EventSourceEntry& event_source_entry) noexcept;
    /**
     * @brief Erase a registration and hand back its slot, which the caller retires outside the lock.
     */
    std::shared_ptr<internal::CallbackSlot> UnregisterInner(const EventSourceHandle event_source_handle) noexcept;
    /**
     * @brief Stop the reactor from invoking @a slot, waiting for an invocation running on another thread.
     */
    void RetireSlot(const std::shared_ptr<internal::CallbackSlot>& slot) noexcept;
    score::Result<void> AccessSources(score::cpp::callback<score::Result<void>(void), 64U> accessor) noexcept;

    score::Result<void> RearmIoSources(internal::EventContainer& received_events) noexcept;
    score::Result<EventTypes> PollActiveEvents(const internal::EventSourceEntry& event_source_entry) noexcept;
    score::Result<std::pair<PulseResult, score::cpp::optional<internal::ReceivedEvent>>> ReceivePulse(
        const std::chrono::milliseconds timeout) noexcept;
    score::Result<UnblockReason> HandlePulses(const std::chrono::milliseconds timeout,
                                              internal::EventContainer& received_events) noexcept;
    score::Result<UnblockReason> ReceiveEvents(const std::chrono::milliseconds timeout,
                                               internal::EventContainer& received_events,
                                               std::unique_lock<std::mutex>& lock) noexcept;
    void CollectCallback(const internal::ReceivedEvent& received_event) noexcept;
    void CollectCallbacks(const internal::EventContainer& received_events) noexcept;
    void RunCallbacks() noexcept;

    /// @brief Handle of the IO source `source`, carrying this dispatcher's tag, as `sources_` keys it.
    EventSourceHandle NativeHandleOf(const std::uint32_t source) const noexcept;
    /// @brief Handle of the pulse code `code`, carrying this dispatcher's tag, as `sources_` keys it.
    EventSourceHandle PulseHandleOf(const std::uint32_t code) const noexcept;

    std::uint16_t max_num_event_sources_;
    DispatcherTag tag_;
    std::int32_t channel_id_;
    std::int32_t connection_id_;
    std::unordered_map<EventSourceHandle, internal::EventSourceEntry> sources_;
    std::unordered_set<EventSourceHandle> io_sources_to_rearm_;
    /**
     * @brief Generation of the last registration of each descriptor number, kept after Unregister().
     * @note Guarded like `sources_`.
     * @note Per number, so a registration always differs from the previous one on that number.
     */
    std::unordered_map<std::uint32_t, std::uint32_t> generations_;
    /**
     * @brief Events one `HandleEvents()` call collects, kept as a member so its capacity is reused across calls.
     */
    internal::EventContainer _internal_received_events_;
    /**
     * @brief Callbacks collected under `dispatcher_mutex_` and run after it is released.
     * @pre Touched only by the thread driving `HandleEvents()`.
     */
    internal::DispatchBatch dispatch_batch_;
    /**
     * @brief Thread that last drove `HandleEvents()`, the only one that ever runs a callback.
     * @pre Stored before `RunCallbacks()` claims a slot, so a caller that sees the claim also sees this store.
     */
    std::atomic<std::thread::id> reactor_thread_{};
    static_assert(std::atomic<std::thread::id>::is_always_lock_free, "a thread id store must not take a lock");

    std::mutex dispatcher_mutex_;
    std::atomic_size_t num_of_accesses_{0};
    std::condition_variable accesses_finished_;
    /**
     * @brief True from before the reactor waits out pending accessors until its pulse receive returns.
     *
     * @pre Set in `ReceiveEvents()` before it reads `num_of_accesses_`, read in `AccessSources()` after raising it.
     * One side then sees the other: the accessor sends the unblock pulse, or the reactor waits the accessor out.
     */
    std::atomic_bool receiving_events_{false};
};

}  // namespace io_dispatcher

#endif  // SCORE_IO_DISPATCHER_QNX_IO_DISPATCHER_IMPL_H
