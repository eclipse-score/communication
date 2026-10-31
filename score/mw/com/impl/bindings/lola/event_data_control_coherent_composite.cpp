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
#include "score/mw/com/impl/bindings/lola/event_data_control_coherent_composite.h"

namespace score::mw::com::impl::lola
{

EventDataControlCoherentComposite::EventDataControlCoherentComposite(
    ProviderEventDataControlCoherentLocalView& qm_control_local) noexcept
    : EventDataControlCoherentComposite{qm_control_local, nullptr}
{
}

EventDataControlCoherentComposite::EventDataControlCoherentComposite(
    ProviderEventDataControlCoherentLocalView& qm_control_local,
    ProviderEventDataControlCoherentLocalView* const asil_b_control_local) noexcept
    : qm_control_local_{qm_control_local}, asil_b_control_local_{asil_b_control_local}, ignore_qm_control_{false}
{
}

SlotIndexType EventDataControlCoherentComposite::AllocateNextSlot() noexcept
{
    if (asil_b_control_local_ == nullptr)
    {
        return qm_control_local_.get().AllocateNextSlot();
    }

    if (ignore_qm_control_)
    {
        return asil_b_control_local_->AllocateNextSlot();
    }

    const auto qm_slot = qm_control_local_.get().AllocateNextSlot();
    const auto asil_b_slot = asil_b_control_local_->AllocateNextSlot();
    if (qm_slot != asil_b_slot)
    {
        // The QM and ASIL-B control sections are expected to always compute the very same next-slot-to-allocate,
        // since only the (trusted) producer ever writes newest_index_ - a kCoherent consumer never writes to its
        // control section at all (see Discard()). A divergence can therefore only be explained by an out-of-band
        // corruption of the QM control section (e.g. by a misbehaving/compromised QM consumer process writing into
        // memory it is only supposed to read). From this point onwards, we no longer trust nor update the QM
        // control section, mirroring EventDataControlComposite's ignore_qm_control_ handling for
        // CoherencyType::kNonCoherent.
        ignore_qm_control_ = true;
    }
    return asil_b_slot;
}

void EventDataControlCoherentComposite::EventReady(const SlotIndexType slot_index,
                                                   const CoherentEventSlotStatus::EventTimeStamp time_stamp) noexcept
{
    if (asil_b_control_local_ != nullptr)
    {
        asil_b_control_local_->EventReady(slot_index, time_stamp);
    }
    if (!ignore_qm_control_)
    {
        qm_control_local_.get().EventReady(slot_index, time_stamp);
    }
}

void EventDataControlCoherentComposite::Discard(const SlotIndexType slot_index) const noexcept
{
    if (asil_b_control_local_ != nullptr)
    {
        asil_b_control_local_->Discard(slot_index);
    }
    if (!ignore_qm_control_)
    {
        qm_control_local_.get().Discard(slot_index);
    }
}

CoherentEventSlotStatus::EventTimeStamp EventDataControlCoherentComposite::GetLatestTimestamp() const noexcept
{
    if (asil_b_control_local_ != nullptr)
    {
        const auto newest_index = asil_b_control_local_->GetNewestIndex();
        return newest_index.has_value() ? (*asil_b_control_local_)[newest_index.value()].GetTimeStamp()
                                        : CoherentEventSlotStatus::INVALID_TIMESTAMP;
    }
    const auto newest_index = qm_control_local_.get().GetNewestIndex();
    return newest_index.has_value() ? qm_control_local_.get()[newest_index.value()].GetTimeStamp()
                                    : CoherentEventSlotStatus::INVALID_TIMESTAMP;
}

bool EventDataControlCoherentComposite::IsQmControlDisconnected() const noexcept
{
    return ignore_qm_control_;
}

}  // namespace score::mw::com::impl::lola
