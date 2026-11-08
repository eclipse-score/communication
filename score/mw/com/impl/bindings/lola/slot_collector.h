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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_CLIENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_CLIENT_H

#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_coherent_local_view.h"
#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_mixed_coherent_local_view.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <functional>
#include <variant>
#include <vector>

namespace score::mw::com::impl::lola
{

/// This class interfaces with the EventDataControl in shared memory to handle finding the slots containing new samples
/// that are pending reception.
class SlotCollector final
{
  public:
    using SlotIndexVector = std::vector<SlotIndexType>;

    class SlotIndices
    {
      public:
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". There are no class invariants to maintain which could be violated by directly accessing member
        // variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        SlotCollector::SlotIndexVector::const_reverse_iterator begin;
        // coverity[autosar_cpp14_m11_0_1_violation]
        SlotCollector::SlotIndexVector::const_reverse_iterator end;
    };

    /// Create a SlotCollector for a CoherencyType::kNonCoherent event/field.
    ///
    /// \param event_data_control EventDataControl to be used for data reception.
    /// \param max_slots Maximum number of samples that will be received in one call to GetNewSamples.
    SlotCollector(ConsumerEventDataControlLocalView<>& event_data_control_local, const std::size_t max_slots) noexcept;

    /// Create a SlotCollector for a CoherencyType::kCoherent event/field.
    ///
    /// \param event_data_control_local EventDataControlCoherent local view to be used for data reception.
    /// \param max_slots Maximum number of samples that will be received in one call to GetNewSamples.
    SlotCollector(ConsumerEventDataControlCoherentLocalView& event_data_control_local,
                  const std::size_t max_slots) noexcept;

    /// Create a SlotCollector for a CoherencyType::kCoherent proxy-side event, whose skeleton-side counterpart was
    /// found to be configured with CoherencyType::kMixed.
    ///
    /// \param event_data_control_local ConsumerEventDataControlMixedCoherentLocalView to be used for data reception.
    /// \param max_slots Maximum number of samples that will be received in one call to GetNewSamples.
    SlotCollector(ConsumerEventDataControlMixedCoherentLocalView& event_data_control_local,
                  const std::size_t max_slots) noexcept;

    SlotCollector(SlotCollector&& other) noexcept = default;
    SlotCollector& operator=(SlotCollector&& other) & noexcept = delete;
    SlotCollector& operator=(const SlotCollector&) & = delete;
    SlotCollector(const SlotCollector&) = delete;

    ~SlotCollector() = default;

    /// \brief Returns the number of new samples a call to GetNewSamples() (given that parameter max_count
    ///        doesn't restrict it) would currently provide.
    ///
    /// \return number of new samples a call to GetNewSamples() would currently provide.
    std::size_t GetNumNewSamplesAvailable() const noexcept;

    /// Get the indices of the slots containing samples that are pending for reception.
    ///
    /// This function is not thread-safe: It may be called from different threads, but the calls need to be
    /// synchronized.
    ///
    /// \param max_count The maximum number of callbacks that shall be executed.
    /// \return Number of samples received.
    SlotIndices GetNewSamplesSlotIndices(const std::size_t max_count) noexcept;

  private:
    /// \brief Collects up to max_count slots (events) in collected_slots_, which have a timestamp > last_ts_
    ///        (are younger than last_ts_) and returns an iterator to one past the last collected (oldest) slot
    /// \param event_data_control_local the CoherencyType::kNonCoherent local view to collect slots from.
    /// \param max_count maximum number of slots to collect.
    /// \return an iterator to one past the oldest collected slot (smallest timestamp).
    SlotIndexVector::const_iterator CollectSlots(ConsumerEventDataControlLocalView<>& event_data_control_local,
                                                 const std::size_t max_count) noexcept;

    /// \brief Overload of CollectSlots() for the CoherencyType::kCoherent flavour of the local view.
    /// \param event_data_control_local the CoherencyType::kCoherent local view to collect slots from.
    /// \param max_count maximum number of slots to collect.
    /// \return an iterator to one past the oldest collected slot (smallest timestamp).
    SlotIndexVector::const_iterator CollectSlots(ConsumerEventDataControlCoherentLocalView& event_data_control_local,
                                                 const std::size_t max_count) noexcept;

    /// \brief Overload of CollectSlots() for the CoherencyType::kCoherent-proxy-reading-a-kMixed-skeleton-event
    ///        flavour of the local view.
    /// \param event_data_control_local the ConsumerEventDataControlMixedCoherentLocalView to collect slots from.
    /// \param max_count maximum number of slots to collect.
    /// \return an iterator to one past the oldest collected slot (smallest timestamp).
    SlotIndexVector::const_iterator CollectSlots(
        ConsumerEventDataControlMixedCoherentLocalView& event_data_control_local,
        const std::size_t max_count) noexcept;

    /// \brief Common implementation shared by the CollectSlots() overloads for both the CoherencyType::kCoherent and
    ///        the CoherencyType::kCoherent-proxy-reading-a-kMixed-skeleton-event flavours of the local view
    ///        (ConsumerEventDataControlCoherentLocalView / ConsumerEventDataControlMixedCoherentLocalView): both
    ///        expose the same GetNewSamplesSlotIndices(last_ts_, output_indices) signature, which already returns
    ///        the found slot indices directly (in ascending/oldest-to-newest order), so no per-slot reference-
    ///        counting loop (as in the CollectSlots() overload for ConsumerEventDataControlLocalView<>) is
    ///        required/possible here.
    /// \param event_data_control_local the coherent-flavoured local view to collect slots from.
    /// \param max_count maximum number of slots to collect.
    /// \return an iterator to one past the oldest collected slot (smallest timestamp).
    template <typename CoherentLikeLocalView>
    SlotIndexVector::const_iterator CollectSlotsCoherentLike(CoherentLikeLocalView& event_data_control_local,
                                                             const std::size_t max_count) noexcept;

    std::variant<std::reference_wrapper<ConsumerEventDataControlLocalView<>>,
                 std::reference_wrapper<ConsumerEventDataControlCoherentLocalView>,
                 std::reference_wrapper<ConsumerEventDataControlMixedCoherentLocalView>>
        event_data_control_local_;
    EventSlotStatus::EventTimeStamp last_ts_;
    SlotIndexVector collected_slots_;  // Pre-allocated scratchpad memory to present the events in-order to the user.
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_CLIENT_H
