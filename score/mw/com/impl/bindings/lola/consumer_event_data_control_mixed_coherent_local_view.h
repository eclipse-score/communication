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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_CONSUMER_EVENT_DATA_CONTROL_MIXED_COHERENT_LOCAL_VIEW_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_CONSUMER_EVENT_DATA_CONTROL_MIXED_COHERENT_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <score/span.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief View class which provides functionality for a CoherencyType::kCoherent consumer interacting with an
/// EventDataControlMixed from the consumer side.
///
/// \details This is the CoherencyType::kMixed counterpart of ConsumerEventDataControlCoherentLocalView, used by a
/// CoherencyType::kCoherent proxy-side event that was found to be configured with CoherencyType::kMixed on the
/// skeleton side (see Proxy::GetEventDataControlCoherentOrMixed()). It implements the exact same ring-buffer search
/// algorithm as ConsumerEventDataControlCoherentLocalView (starting from newest_index_ and walking backwards until a
/// known/old timestamp or a full wrap-around is reached, see Nautilus_LoLa.md "Mixed Intra/Inter-Chain" / "Consuming
/// Steps for coherent consumer"), but reads the slot timestamp out of the full 8 byte atomic EventSlotStatus (the
/// same CONTROL segment layout CoherencyType::kNonCoherent uses) instead of the 4 byte CoherentEventSlotStatus used
/// by a "pure" CoherencyType::kCoherent event.
///
/// Even though the slots are of atomic type (as CoherencyType::kNonCoherent consumers, which may run concurrently
/// with the producer, also access them), this view never needs any particular memory order stronger than relaxed for
/// its loads: a single atomic load already yields an internally-consistent (timestamp, refcount) snapshot, and the
/// guarantee that the producer and all coherent consumers of a kCoherent/kMixed event never execute concurrently
/// means no further synchronization with the producer's writes is required. As with
/// ConsumerEventDataControlCoherentLocalView, no per-slot reference-counting/dereference step is performed, which is
/// why this class, unlike ConsumerEventDataControlLocalView, has no transaction-log related member/logic at all.
class ConsumerEventDataControlMixedCoherentLocalView final
{
  public:
    using LocalEventControlSlots = score::cpp::span<ControlSlotType>;

    explicit ConsumerEventDataControlMixedCoherentLocalView(EventDataControlMixed& event_data_control) noexcept;

    ~ConsumerEventDataControlMixedCoherentLocalView() noexcept = default;

    ConsumerEventDataControlMixedCoherentLocalView(const ConsumerEventDataControlMixedCoherentLocalView&) = delete;
    ConsumerEventDataControlMixedCoherentLocalView& operator=(const ConsumerEventDataControlMixedCoherentLocalView&) =
        delete;
    ConsumerEventDataControlMixedCoherentLocalView(ConsumerEventDataControlMixedCoherentLocalView&&) noexcept = delete;
    ConsumerEventDataControlMixedCoherentLocalView& operator=(
        ConsumerEventDataControlMixedCoherentLocalView&& other) noexcept = delete;

    /// \brief Collects the slot indices of all samples published (via EventDataControlMixedComposite::EventReady())
    /// since last_search_time, in ascending (oldest to newest) order.
    ///
    /// \details See ConsumerEventDataControlCoherentLocalView::GetNewSamplesSlotIndices() for the full algorithm
    /// description; this is the byte-for-byte same search, just reading timestamps out of EventSlotStatus instead of
    /// CoherentEventSlotStatus.
    ///
    /// \param last_search_time the reference timestamp used in a previous call to this function (or
    ///        EventSlotStatus::INVALID_TIMESTAMP, if this is the first call).
    /// \param output_indices caller-provided buffer the found slot indices are written into, in ascending
    ///        (oldest to newest) order. At most output_indices.size() slot indices are collected.
    /// \return the number of slot indices written into output_indices (<= output_indices.size()).
    std::size_t GetNewSamplesSlotIndices(const EventSlotStatus::EventTimeStamp last_search_time,
                                         const score::cpp::span<SlotIndexType> output_indices) const noexcept;

    /// \brief Returns number/count of events within event slots, which are newer than the given timestamp.
    /// \param reference_time given reference timestamp.
    /// \return number/count of available events, which are newer than the given reference_time.
    std::size_t GetNumNewEvents(const EventSlotStatus::EventTimeStamp reference_time) const noexcept;

    /// \brief Directly access EventSlotStatus for one specific slot (read-only, relaxed atomic load).
    EventSlotStatus operator[](const SlotIndexType slot_index) const noexcept;

    /// \brief Returns the index of the slot holding the most recently published sample, or an empty optional, if no
    /// sample has been published yet.
    std::optional<SlotIndexType> GetNewestIndex() const noexcept;

    /// \brief Returns the max sample slots set on creation of EventDataControlMixed
    std::size_t GetMaxSampleSlots() const noexcept
    {
        return state_slots_.size();
    }

  private:
    LocalEventControlSlots state_slots_;

    /// \brief Pointer to the newest_index_ member of the (shared-memory resident) EventDataControlMixed. Accessed
    /// directly (no further indirection necessary, as it is a plain member of an object which was already resolved
    /// once, via the event_data_control reference handed to the constructor).
    const EventDataControlMixed* event_data_control_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_CONSUMER_EVENT_DATA_CONTROL_MIXED_COHERENT_LOCAL_VIEW_H
