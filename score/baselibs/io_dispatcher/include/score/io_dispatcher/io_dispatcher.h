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
 * Abstract interface of the io_dispatcher (reactor).
 *
 * A thread-safe reactor: register callbacks for IO sources, block in HandleEvents()
 * until a source is ready / the timeout expires / a signal arrives / Unblock() is
 * called, and dispatch the ready callbacks synchronously.
 *
 * All operations are thread-safe and may be called from within a callback, EXCEPT
 * HandleEvents(), which must be driven by a single thread and never called from a
 * callback of this object.
 ********************************************************************************/

#ifndef SCORE_IO_DISPATCHER_IO_DISPATCHER_H
#define SCORE_IO_DISPATCHER_IO_DISPATCHER_H

#include "score/io_dispatcher/io_dispatcher_types.h"
#include "score/result/result.h"
#include <chrono>

namespace io_dispatcher
{

/**
 * @brief The registration half of a reactor; HandleEvents() and Unblock() stay with the owner of the waiting thread.
 */
class EventSourceRegistry
{
  public:
    EventSourceRegistry() = default;
    virtual ~EventSourceRegistry() = default;

    EventSourceRegistry(const EventSourceRegistry&) = delete;
    EventSourceRegistry& operator=(const EventSourceRegistry&) = delete;
    EventSourceRegistry(EventSourceRegistry&&) = delete;
    EventSourceRegistry& operator=(EventSourceRegistry&&) = delete;

    /**
     * @brief Register a callback for an IO source. Only one callback per source.
     * @error kApiError io_source is already registered on this dispatcher, or callback is empty.
     * @error kInvalidHandle io_source is negative or otherwise not valid.
     * @error kResource no free slot to register another callback.
     * @note A source registered with no events is never dispatched, whatever its callback holds.
     * @note On an error the callback is destroyed; nothing is handed back to retry with.
     */
    virtual score::Result<EventSourceHandle> Register(const NativeHandle io_source,
                                                      const EventTypes events,
                                                      CallbackType callback) noexcept = 0;

    /**
     * @brief Replace the set of active event types for a registered callback.
     */
    virtual score::Result<void> SetMonitoredEvents(const EventSourceHandle event_source_handle,
                                                   const EventTypes events) noexcept = 0;

    /**
     * @brief Replace the callback target.
     */
    virtual score::Result<void> SetCallbackTarget(const EventSourceHandle event_source_handle,
                                                  CallbackType callback) noexcept = 0;

    /**
     * @brief Unregister a callback and destroy it, with everything it captured.
     * @note From another thread, waits for a running invocation; the callback is destroyed before this returns.
     * @note From inside the callback itself, the callback is destroyed once that invocation returns.
     * @note Once this returns, the dispatcher no longer waits on the source, so a socket closed then frees its port.
     */
    virtual void Unregister(const EventSourceHandle event_source_handle) noexcept = 0;
};

/**
 * @brief Abstract reactor interface (OS-agnostic).
 */
class IoDispatcher : public EventSourceRegistry
{
  public:
    /**
     * @brief Wait for events and dispatch ready callbacks.
     * @param timeout Time to wait. Zero or negative returns immediately; values above the maximum are clamped.
     */
    virtual score::Result<UnblockReason> HandleEvents(const std::chrono::milliseconds timeout) noexcept = 0;

    /**
     * @brief Cause a blocked (or next) HandleEvents() to return.
     */
    virtual void Unblock() noexcept = 0;
};

}  // namespace io_dispatcher

#endif  // SCORE_IO_DISPATCHER_IO_DISPATCHER_H
