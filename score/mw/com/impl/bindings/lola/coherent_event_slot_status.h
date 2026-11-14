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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_COHERENT_EVENT_SLOT_STATUS_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_COHERENT_EVENT_SLOT_STATUS_H

#include <cstdint>

namespace score::mw::com::impl::lola
{

/// \brief Control-block for one slot of a CoherencyType::kCoherent (or kMixed, from the coherent-consumer's point of
/// view) event.
///
/// \details Unlike EventSlotStatus, which is used for CoherencyType::kNonCoherent and has to additionally carry a
/// reference-count and be stored inside a std::atomic (because producer and consumers of a kNonCoherent event may run
/// concurrently and need to synchronize via the CONTROL segment), a CoherentEventSlotStatus only carries a timestamp.
/// It is a plain (non-atomic) 4 byte value: for a kCoherent event, the caller guarantees (e.g. via activity-chain
/// scheduling) that the producer and all of its consumers never execute concurrently for this event. Consequently, no
/// atomics / compare-exchange are required and consumers never need to alter this state (no reference-counting), which
/// is also why no transaction-log based crash-recovery is required for kCoherent events (see EventControlCoherent).
class CoherentEventSlotStatus final
{
  public:
    /// \brief A EventTimeStamp represents a strictly monotonic counter that is increased every time an event is sent.
    using EventTimeStamp = std::uint32_t;

    /// \brief Represents an invalid/absent timestamp. A slot with this value has never been written.
    static constexpr EventTimeStamp INVALID_TIMESTAMP = 0U;

    /// \brief The first timestamp written to a slot when an event is sent for the first time.
    static constexpr EventTimeStamp FIRST_VALID_TIMESTAMP = 1U;

    /// \brief If default constructed, the slot is invalid (never written).
    CoherentEventSlotStatus() noexcept : CoherentEventSlotStatus{INVALID_TIMESTAMP} {}
    explicit CoherentEventSlotStatus(const EventTimeStamp timestamp) noexcept : timestamp_{timestamp} {}

    CoherentEventSlotStatus(const CoherentEventSlotStatus&) noexcept = default;
    CoherentEventSlotStatus(CoherentEventSlotStatus&&) noexcept = default;
    CoherentEventSlotStatus& operator=(const CoherentEventSlotStatus&) & noexcept = default;
    CoherentEventSlotStatus& operator=(CoherentEventSlotStatus&&) & noexcept = default;
    ~CoherentEventSlotStatus() noexcept = default;

    bool IsInvalid() const noexcept
    {
        return timestamp_ == INVALID_TIMESTAMP;
    }

    void MarkInvalid() noexcept
    {
        timestamp_ = INVALID_TIMESTAMP;
    }

    EventTimeStamp GetTimeStamp() const noexcept
    {
        return timestamp_;
    }

    void SetTimeStamp(const EventTimeStamp timestamp) noexcept
    {
        timestamp_ = timestamp;
    }

  private:
    EventTimeStamp timestamp_;
};

static_assert(sizeof(CoherentEventSlotStatus) == sizeof(CoherentEventSlotStatus::EventTimeStamp),
              "CoherentEventSlotStatus shall only contain the 4 byte timestamp part of EventSlotStatus.");

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_COHERENT_EVENT_SLOT_STATUS_H
