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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_TEST_RESOURCES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_TEST_RESOURCES_H

#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"

#include <score/optional.hpp>

namespace score::mw::com::impl::lola
{

class EventDataControlCompositeAttorney
{
  public:
    EventDataControlCompositeAttorney(EventDataControlComposite& event_data_control_composite) noexcept;

    /// \brief Prepares the underlying EventDataControlComposite (its contained EventDataControls) in a way, that the
    ///        next call to AllocateNextSlot() will return the given expected_result
    /// \param expected_result to be expected result for AllocateNextSlot()
    void PrepareAllocateNextSlot(const std::pair<score::cpp::optional<SlotIndexType>, bool> expected_result) noexcept;
    /// \brief Prepares the underlying EventDataControlComposite in way, that it returns the expected_result in the next
    ///        call to IsQmControlDisconnected()
    /// \param expected_result to be expected result for IsQmControlDisconnected()
    void SetQmControlDisconnected(const bool expected_result) noexcept;
    /// \brief return underlying states of contained EventDataControls
    /// \param slot_index index for which the EventSlotStatus shall be returned
    /// \return a pair, where first contains the EventSlotStatus of qm control and second (optionally) the
    /// EventSlotStatus of the asil_b control.
    std::pair<EventSlotStatus, score::cpp::optional<EventSlotStatus>> GetSlotStatus(
        const SlotIndexType slot_index) const noexcept;

  private:
    EventDataControlComposite& event_data_control_composite_;
};

class EventDataControlAttorney
{
  public:
    EventDataControlAttorney(EventDataControl& event_data_control) noexcept;

    /// \brief Prepares the underlying EventDataControl in a way, that the
    ///        next call to AllocateNextSlot() will return the given expected_result
    /// \param expected_result to be expected result for AllocateNextSlot()
    void PrepareAllocateNextSlot(const score::cpp::optional<SlotIndexType> expected_result) noexcept;

    /// \brief Prepares the underlying EventDataControl in a way, that the next call to ReferenceNextEvent() will return
    ///        the given expected_result.
    /// \details The function prepares the underlying state-slots in the following way:
    ///          All slots are set to INVALID. In case expected_result.has_value() == true, then the slot identified by
    ///          expected_result.value() has its timestamp set to last_search_time + 1 and its reference count is set to
    ///          0.
    /// \param expected_result to be expected result for ReferenceNextEvent()
    /// \param last_search_time see ReferenceNextEvent()
    /// \param upper_limit see ReferenceNextEvent()
    void PrepareReferenceNextEvent(
        const score::cpp::optional<SlotIndexType> expected_result,
        const EventSlotStatus::EventTimeStamp last_search_time,
        const EventSlotStatus::EventTimeStamp upper_limit = EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX) noexcept;

    /// \brief Prepares the underlying EventDataControl in a way, that the next call to GetNumNewEvents() will return
    /// the given expected_result
    /// \details The function prepares the underlying state-slots in the following way:
    ///          For the first expected_result slots its timestamps are set to reference_time + 1, + 2, and so on.
    ///          For the remaining slots, the status is set to invalid.
    /// \param expected_result to be expected result for GetNumNewEvents()
    /// \param reference_time GetNumNewEvents()
    void PrepareGetNumNewEvents(const std::size_t expected_result,
                                const EventSlotStatus::EventTimeStamp reference_time) noexcept;

  private:
    EventDataControl& event_data_control_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_TEST_RESOURCES_H
