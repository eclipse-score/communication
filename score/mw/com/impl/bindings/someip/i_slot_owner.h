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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_I_SLOT_OWNER_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_I_SLOT_OWNER_H

#include "score/mw/com/impl/bindings/someip/event_data_storage.h"

namespace score::mw::com::impl::someip
{

/// \brief Interface of the entity which hands out sample slots and takes them back.
///
/// \details SampleAllocateePtr has to return its slot when it is destroyed without a preceding Send(). It refers to
///          its owner through this interface rather than through SkeletonEvent directly, because SkeletonEvent hands
///          out SampleAllocateePtr and would otherwise form a dependency cycle. This mirrors LoLa, where
///          SampleAllocateePtr refers to the EventDataControlComposite instead of to lola::SkeletonEvent.
class ISlotOwner
{
  public:
    ISlotOwner() = default;
    virtual ~ISlotOwner();

    ISlotOwner(const ISlotOwner&) = delete;
    ISlotOwner(ISlotOwner&&) noexcept = delete;
    ISlotOwner& operator=(const ISlotOwner&) & = delete;
    ISlotOwner& operator=(ISlotOwner&&) & noexcept = delete;

    /// \brief Returns a slot which was handed out by Allocate() so that it can be reused.
    virtual void DiscardSlot(SlotIndexType slot_index) noexcept = 0;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_I_SLOT_OWNER_H
