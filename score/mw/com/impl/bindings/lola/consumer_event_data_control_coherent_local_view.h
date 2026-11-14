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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_CONSUMER_EVENT_DATA_CONTROL_COHERENT_LOCAL_VIEW_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_CONSUMER_EVENT_DATA_CONTROL_COHERENT_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/coherent_event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_coherent.h"

#include <score/span.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief View class which provides functionality for interacting with EventDataControlCoherent from the consumer
/// side.
///
/// \details This is the CoherencyType::kCoherent counterpart of ConsumerEventDataControlLocalView. Since the caller
/// guarantees (e.g. via activity-chain scheduling) that the producer and all of its consumers of a kCoherent event
/// never execute concurrently, a consumer never needs to alter any state in the CONTROL segment (no reference-
/// counting / no dereference step, unlike the CoherencyType::kNonCoherent case) which is also why this class, unlike
/// ConsumerEventDataControlLocalView, has no transaction-log related member/logic at all (no crash-recovery is
/// required for events a consumer never writes to).
///
/// As with ConsumerEventDataControlLocalView, the data in EventDataControlCoherent is created / opened once during
/// Proxy creation, and is then accessed during runtime via this local view, to avoid repeated (performance impacting)
/// OffsetPtr dereferences.
class ConsumerEventDataControlCoherentLocalView final
{
  public:
    using LocalEventControlSlots = score::cpp::span<CoherentControlSlotType>;

    explicit ConsumerEventDataControlCoherentLocalView(EventDataControlCoherent& event_data_control) noexcept;

    ~ConsumerEventDataControlCoherentLocalView() noexcept = default;

    ConsumerEventDataControlCoherentLocalView(const ConsumerEventDataControlCoherentLocalView&) = delete;
    ConsumerEventDataControlCoherentLocalView& operator=(const ConsumerEventDataControlCoherentLocalView&) = delete;
    ConsumerEventDataControlCoherentLocalView(ConsumerEventDataControlCoherentLocalView&&) noexcept = delete;
    ConsumerEventDataControlCoherentLocalView& operator=(ConsumerEventDataControlCoherentLocalView&& other) noexcept =
        delete;

    /// \brief Collects the slot indices of all samples published (via EventReady()) since last_search_time, in
    /// ascending (oldest to newest) order.
    ///
    /// \details Starting from the slot referenced by newest_index_, this walks backwards (in ring-buffer order)
    /// until either: a slot with a timestamp <= last_search_time is reached (i.e. a sample already known to the
    /// caller from a previous call), max_count samples have been collected, or all slots have been visited (which
    /// can only happen on the very first call, when last_search_time equals
    /// CoherentEventSlotStatus::INVALID_TIMESTAMP).
    ///
    /// \param last_search_time the reference timestamp used in a previous call to this function (or
    ///        CoherentEventSlotStatus::INVALID_TIMESTAMP, if this is the first call).
    /// \param output_indices caller-provided buffer the found slot indices are written into, in ascending
    ///        (oldest to newest) order. At most output_indices.size() slot indices are collected.
    /// \return the number of slot indices written into output_indices (<= output_indices.size()).
    std::size_t GetNewSamplesSlotIndices(const CoherentEventSlotStatus::EventTimeStamp last_search_time,
                                         const score::cpp::span<SlotIndexType> output_indices) const noexcept;

    /// \brief Returns number/count of events within event slots, which are newer than the given timestamp.
    /// \param reference_time given reference timestamp.
    /// \return number/count of available events, which are newer than the given reference_time.
    std::size_t GetNumNewEvents(const CoherentEventSlotStatus::EventTimeStamp reference_time) const noexcept;

    /// \brief Directly access CoherentEventSlotStatus for one specific slot
    CoherentEventSlotStatus operator[](const SlotIndexType slot_index) const noexcept;

    /// \brief Returns the index of the slot holding the most recently published sample, or an empty optional, if no
    /// sample has been published yet.
    std::optional<SlotIndexType> GetNewestIndex() const noexcept;

    /// \brief Returns the max sample slots set on creation of EventDataControlCoherent
    std::size_t GetMaxSampleSlots() const noexcept
    {
        return state_slots_.size();
    }

  private:
    LocalEventControlSlots state_slots_;

    /// \brief Reference to the newest_index_ member of the (shared-memory resident) EventDataControlCoherent.
    const std::optional<SlotIndexType>& newest_index_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_CONSUMER_EVENT_DATA_CONTROL_COHERENT_LOCAL_VIEW_H
