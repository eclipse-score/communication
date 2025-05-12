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

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include "score/mw/com/impl/com_error.h"

#include <functional>
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

    /// Create a SlotCollector for the specified service instance and event.
    ///
    /// \param event_data_control EventDataControl to be used for data reception.
    /// \param max_slots Maximum number of samples that will be received in one call to GetNewSamples.
    SlotCollector(EventDataControl& event_data_control,
                  const std::size_t max_slots,
                  TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept;

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
    SlotIndexVector::const_iterator CollectSlots(const std::size_t max_count) noexcept;

    std::reference_wrapper<EventDataControl> event_data_control_;
    EventSlotStatus::EventTimeStamp last_ts_;
    SlotIndexVector collected_slots_;  // Pre-allocated scratchpad memory to present the events in-order to the user.
    TransactionLogSet::TransactionLogIndex transaction_log_index_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_CLIENT_H
