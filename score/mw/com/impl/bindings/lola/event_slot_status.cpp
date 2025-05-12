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
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

namespace score::mw::com::impl::lola
{
namespace
{
/// \brief Indicates that the event was never written.
constexpr EventSlotStatus::value_type INVALID_EVENT = 0U;

/// \brief Indicates that the event data is altered and one should not increase the refcount.
constexpr EventSlotStatus::value_type IN_WRITING = std::numeric_limits<EventSlotStatus::SubscriberCount>::max();
}  // namespace

EventSlotStatus::EventSlotStatus(const value_type init_val) noexcept : data_{init_val} {}

EventSlotStatus::EventSlotStatus(const EventTimeStamp timestamp, const SubscriberCount refcount) noexcept : data_{0U}
{
    SetTimeStamp(timestamp);
    SetReferenceCount(refcount);
}

auto EventSlotStatus::GetReferenceCount() const noexcept -> SubscriberCount
{
    return static_cast<SubscriberCount>(data_ & 0x00000000FFFFFFFFU);  // ignore first 4 byte
}

auto EventSlotStatus::GetTimeStamp() const noexcept -> EventTimeStamp
{
    return static_cast<EventTimeStamp>(data_ >> 32U);  // ignore last 4 byte
}

auto EventSlotStatus::IsInvalid() const noexcept -> bool
{
    return data_ == INVALID_EVENT;
}

auto EventSlotStatus::IsInWriting() const noexcept -> bool
{
    return data_ == IN_WRITING;
}

auto EventSlotStatus::MarkInWriting() noexcept -> void
{
    data_ = IN_WRITING;
}

auto EventSlotStatus::MarkInvalid() noexcept -> void
{
    data_ = INVALID_EVENT;
}

auto EventSlotStatus::SetTimeStamp(const EventTimeStamp time_stamp) noexcept -> void
{
    data_ = static_cast<value_type>(time_stamp) << 32U;
}

auto EventSlotStatus::SetReferenceCount(const SubscriberCount ref_count) noexcept -> void
{
    data_ = (data_ & 0xFFFFFFFF00000000U) | static_cast<value_type>(ref_count);
}

auto EventSlotStatus::IsTimeStampBetween(const EventTimeStamp min_timestamp,
                                         const EventTimeStamp max_timestamp) const noexcept -> bool
{
    // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states:"The operands of a logical && or \\ shall be
    // parenthesized if the operands contain binary operators".
    // This a false-positive, all operands are parenthesized.
    // A bug ticket has been created to track this: [Ticket-165315](broken_link_j/Ticket-165315)
    // coverity[autosar_cpp14_a5_2_6_violation : FALSE]
    return ((IsInWriting() == false) && (IsInvalid() == false) && (GetTimeStamp() > min_timestamp) &&
            (GetTimeStamp() < max_timestamp));
}

// Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
// references to private or protected data owned by the class.".
// The result reference of the cast operator to the underlying data type is used by atomic operations in a codebase,
// e.i. compare_exchange_weak and compare_exchange_strong. This a simple way without the need to extend these atomic
// operations with corresponding template specializations
// coverity[autosar_cpp14_a9_3_1_violation]
EventSlotStatus::operator value_type&() noexcept
{
    // coverity[autosar_cpp14_a9_3_1_violation]
    return data_;
}

EventSlotStatus::operator const value_type&() const noexcept
{
    return data_;
}

auto EventSlotStatus::IsUsed() const noexcept -> bool
{
    return ((GetReferenceCount() != static_cast<SubscriberCount>(0)) || IsInWriting());
}

}  // namespace score::mw::com::impl::lola
