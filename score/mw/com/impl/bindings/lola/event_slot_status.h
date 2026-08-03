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

    /// \brief Represents an invalid/absent timestamp. A slot with this value has never been written.
    static constexpr EventTimeStamp INVALID_TIMESTAMP = 0U;

    /// \brief The first timestamp written to a slot when an event is sent for the first time.
    static constexpr EventTimeStamp FIRST_VALID_TIMESTAMP = 1U;

    /// \brief The highest possible value that EventTimeStamp can reach
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Variable is used in EventDataControl.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    static constexpr EventTimeStamp TIMESTAMP_MAX = std::numeric_limits<EventTimeStamp>::max();

    /// \brief If default constructed, SlotStatus is invalid
    EventSlotStatus() noexcept : EventSlotStatus{INVALID_TIMESTAMP} {}
    explicit EventSlotStatus(const value_type init_val) noexcept : data_{init_val} {}
    EventSlotStatus(const EventTimeStamp timestamp, const SubscriberCount refcount) noexcept : data_{0U}
    {
        SetTimeStamp(timestamp);
        SetReferenceCount(refcount);
    }
    EventSlotStatus(const EventSlotStatus&) noexcept = default;
    EventSlotStatus(EventSlotStatus&&) noexcept = default;
    EventSlotStatus& operator=(const EventSlotStatus&) & noexcept = default;
    EventSlotStatus& operator=(EventSlotStatus&&) & noexcept = default;

    ~EventSlotStatus() noexcept = default;

    // Hot-path optimization: all accessors below are defined inline in this header (rather than out-of-line in the
    // .cpp) so that they can be inlined into the GetNewSamples() sample-collection loop, avoiding a function call per
    // slot for these trivial bit-manipulation operations.

    bool IsInvalid() const noexcept
    {
        return data_ == kInvalidEvent;
    }
    bool IsInWriting() const noexcept
    {
        return data_ == kInWriting;
    }

    void MarkInWriting() noexcept
    {
        data_ = kInWriting;
    }
    void MarkInvalid() noexcept
    {
        data_ = kInvalidEvent;
    }

    SubscriberCount GetReferenceCount() const noexcept
    {
        return static_cast<SubscriberCount>(data_ & 0x00000000FFFFFFFFU);  // ignore first 4 byte
    }
    EventTimeStamp GetTimeStamp() const noexcept
    {
        return static_cast<EventTimeStamp>(data_ >> 32U);  // ignore last 4 byte
    }

    void SetTimeStamp(const EventTimeStamp time_stamp) noexcept
    {
        data_ = static_cast<value_type>(time_stamp) << 32U;
    }
    void SetReferenceCount(const SubscriberCount ref_count) noexcept
    {
        data_ = (data_ & 0xFFFFFFFF00000000U) | static_cast<value_type>(ref_count);
    }

    // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
    // references to private or protected data owned by the class.".
    // The result reference of the cast operator to the underlying data type is used by atomic operations in a codebase,
    // e.i. compare_exchange_weak and compare_exchange_strong. This a simple way without the need to extend these atomic
    // operations with corresponding template specializations
    // coverity[autosar_cpp14_a9_3_1_violation]
    explicit operator value_type&() & noexcept
    {
        // coverity[autosar_cpp14_a9_3_1_violation]
        return data_;
    }
    explicit operator const value_type&() const& noexcept
    {
        return data_;
    }

    bool IsUsed() const noexcept
    {
        return ((GetReferenceCount() != static_cast<SubscriberCount>(0)) || IsInWriting());
    }

    /// Returns whether the timestamp is valid and within a certain range.
    ///
    /// \param min_tstmp Minimum timestamp
    /// \param max_tstmp Maximum timestamp
    /// \return true if timestamp is valid and within ]min; max[, false otherwise
    bool IsTimeStampBetween(const EventTimeStamp min_timestamp, const EventTimeStamp max_timestamp) const noexcept
    {
        // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states:"The operands of a logical && or \\ shall be
        // parenthesized if the operands contain binary operators".
        // This a false-positive, all operands are parenthesized.
        // A bug ticket has been created to track this: [Ticket-165315](broken_link_j/Ticket-165315)
        // coverity[autosar_cpp14_a5_2_6_violation : FALSE]
        return ((IsInWriting() == false) && (IsInvalid() == false) && (GetTimeStamp() > min_timestamp) &&
                (GetTimeStamp() < max_timestamp));
    }

  private:
    /// \brief Indicates that the event was never written.
    static constexpr value_type kInvalidEvent = 0U;

    /// \brief Indicates that the event data is altered and one should not increase the refcount.
    static constexpr value_type kInWriting = std::numeric_limits<SubscriberCount>::max();

    value_type data_;
};

static_assert(sizeof(EventSlotStatus) <= sizeof(std::uint64_t),
              "EventSlotStatus must fit inside a std::atomic which is currently 64 bit.");
}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_SLOT_STATUS_H
