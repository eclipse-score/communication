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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_SLOT_STATUS_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_SLOT_STATUS_H

#include <cstdint>
#include <limits>
#include <type_traits>

namespace score::mw::com::impl::lola
{

/// \brief Each event consists out of two things. The actual event data and a control block. EventSlotStatus represents
/// the control block. It provides meta-information for an event. This class acts as an easy access towards this
/// meta-information.
///
/// \details This data structure needs to fit into std::atomic. Thus it's size shall not exceed the machine word size.
/// Currently this is 64-bit. It shall be noted that we cannot protect timestamp and refcount independent with e.g.
/// std::atomic, since we would never be able to resolve all possible race-conditions that occur. Both data points need
/// to be updated atomically.
class EventSlotStatus final
{
  public:
    /// \brief A EventTimeStamp represents a strictly monotonic counter that is increased every time an event is sent.
    using EventTimeStamp = std::uint32_t;

    /// \brief The SubscriberCount represents the number of proxies that currently use a given event slot.
    using SubscriberCount = std::uint32_t;

    /// \brief The value_type represents the underlying data type of this structure
    using value_type = std::uint64_t;

    /// \brief The highest possible value that EventTimeStamp can reach
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Variable is used in EventDataControlImpl.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    static constexpr EventTimeStamp TIMESTSCORE_LANGUAGE_FUTURECPP_MAX = std::numeric_limits<EventTimeStamp>::max();

    /// \brief If default constructed, SlotStatus is invalid
    EventSlotStatus() noexcept = default;
    explicit EventSlotStatus(const value_type init_val) noexcept;
    EventSlotStatus(const EventTimeStamp timestamp, const SubscriberCount refcount) noexcept;
    EventSlotStatus(const EventSlotStatus&) noexcept = default;
    EventSlotStatus(EventSlotStatus&&) noexcept = default;
    EventSlotStatus& operator=(const EventSlotStatus&) & noexcept = default;
    EventSlotStatus& operator=(EventSlotStatus&&) & noexcept = default;

    ~EventSlotStatus() noexcept = default;

    bool IsInvalid() const noexcept;
    bool IsInWriting() const noexcept;

    void MarkInWriting() noexcept;
    void MarkInvalid() noexcept;

    SubscriberCount GetReferenceCount() const noexcept;
    EventTimeStamp GetTimeStamp() const noexcept;

    void SetTimeStamp(const EventTimeStamp time_stamp) noexcept;
    void SetReferenceCount(const SubscriberCount ref_count) noexcept;

    explicit operator value_type&() noexcept;
    explicit operator const value_type&() const noexcept;

    bool IsUsed() const noexcept;

    /// Returns whether the timestamp is valid and within a certain range.
    ///
    /// \param min_tstmp Minimum timestamp
    /// \param max_tstmp Maximum timestamp
    /// \return true if timestamp is valid and within ]min; max[, false otherwise
    bool IsTimeStampBetween(const EventTimeStamp min_timestamp, const EventTimeStamp max_timestamp) const noexcept;

  private:
    value_type data_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_SLOT_STATUS_H
