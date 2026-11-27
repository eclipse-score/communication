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
 * io_dispatcher: an IO event demultiplexer built on the score baselibs.
 * Common types shared by the Linux and QNX backends.
 ********************************************************************************/

#ifndef SCORE_IO_DISPATCHER_IO_DISPATCHER_TYPES_H
#define SCORE_IO_DISPATCHER_IO_DISPATCHER_TYPES_H

#include "score/callback.hpp"
#include <cstdint>
#include <limits>

namespace io_dispatcher
{

/**
 * @brief Native OS handle (file descriptor). Raw int, per score convention.
 */
using NativeHandle = std::int32_t;

/**
 * @brief Invalid native handle value.
 */
constexpr NativeHandle kInvalidNativeHandle{-1};

/**
 * @brief Handle identifying a registered event source.
 * @note Opaque and backend specific, except for bits 48..50: they carry the issuing dispatcher's DispatcherTag,
 *       and DispatcherTagOf() is the only decoding a caller may rely on.
 */
using EventSourceHandle = std::uint64_t;

/**
 * @brief Invalid event source handle value.
 */
constexpr EventSourceHandle kInvalidEventSourceHandle{std::numeric_limits<EventSourceHandle>::max()};

/**
 * @brief Value a dispatcher stamps into every handle it issues, so a set of dispatchers can tell their handles apart.
 */
using DispatcherTag = std::uint8_t;

/**
 * @brief Largest DispatcherTag a dispatcher accepts.
 */
constexpr DispatcherTag kMaxDispatcherTag{7U};

namespace internal
{
constexpr std::uint32_t kDispatcherTagOffset{48U};
constexpr EventSourceHandle kDispatcherTagMask{static_cast<EventSourceHandle>(kMaxDispatcherTag)
                                               << kDispatcherTagOffset};

/**
 * @brief Returns `handle` with its tag bits set to `tag`.
 * @pre `tag` is at most kMaxDispatcherTag, and the backend keeps bits 48..50 of `handle` free for it.
 */
constexpr EventSourceHandle WithDispatcherTag(const EventSourceHandle handle, const DispatcherTag tag) noexcept
{
    return (handle & ~kDispatcherTagMask) | (static_cast<EventSourceHandle>(tag) << kDispatcherTagOffset);
}
}  // namespace internal

/**
 * @brief Returns the tag of the dispatcher that issued `handle`.
 * @note kInvalidEventSourceHandle decodes to kMaxDispatcherTag; compare against it first.
 */
constexpr DispatcherTag DispatcherTagOf(const EventSourceHandle handle) noexcept
{
    return static_cast<DispatcherTag>((handle & internal::kDispatcherTagMask) >> internal::kDispatcherTagOffset);
}

#if defined(__QNX__)
/**
 * @brief QNX pulse code carried by a generic pulse event.
 */
using PulseCode = std::int8_t;
#endif

/**
 * @brief Set of IO event types an event source can be registered/notified for.
 */
class EventTypes final
{
  public:
    bool HasReadEvent() const noexcept
    {
        return read_event_;
    }

    bool HasWriteEvent() const noexcept
    {
        return write_event_;
    }

    EventTypes& SetReadEvent(const bool enable) noexcept
    {
        read_event_ = enable;
        return *this;
    }

    EventTypes& SetWriteEvent(const bool enable) noexcept
    {
        write_event_ = enable;
        return *this;
    }

#if defined(__QNX__)
    bool HasPulseEvent() const noexcept
    {
        return pulse_event_;
    }

    PulseCode GetPulseCode() const noexcept
    {
        return pulse_code_;
    }

    EventTypes& SetPulseEvent(const bool enable) noexcept
    {
        pulse_event_ = enable;
        return *this;
    }

    EventTypes& SetPulseCode(const PulseCode code) noexcept
    {
        pulse_code_ = code;
        return *this;
    }

    bool HasAnyEvent() const noexcept
    {
        return read_event_ || write_event_ || pulse_event_;
    }
#else
    bool HasAnyEvent() const noexcept
    {
        return read_event_ || write_event_;
    }
#endif

  private:
    bool read_event_{false};
    bool write_event_{false};
#if defined(__QNX__)
    bool pulse_event_{false};
    PulseCode pulse_code_{0};
#endif
};

/**
 * @brief Callback invoked when a registered IO source becomes ready.
 *
 * Move-only, heap-free fixed capacity (64 bytes).
 * The callback shall not throw.
 * The callback destructor shall not call the reactor from its destructor.
 */
using CallbackType = score::cpp::callback<void(EventSourceHandle, EventTypes), 64U>;

/**
 * @brief Reason why HandleEvents() returned.
 */
enum class UnblockReason : std::uint32_t
{
    kEventsHandledOrUnblock, /*!< Events were handled or the dispatcher was unblocked. */
    kTimeout,                /*!< The timeout expired. */
    kSignal                  /*!< A signal interrupted the wait. */
};

}  // namespace io_dispatcher

#endif  // SCORE_IO_DISPATCHER_IO_DISPATCHER_TYPES_H
