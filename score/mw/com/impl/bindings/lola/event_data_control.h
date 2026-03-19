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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include "score/containers/dynamic_array.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

namespace score::mw::com::impl::lola
{

/// \brief EventDataControl encapsulates the overall control information for one event. It is stored in Shared Memory.
///
/// \details EventDataControl holds a dynamic array of multiple slots, which hold EventSlotStatus. The
/// event has another equally sized dynamic array of slots which will contain the data. Both data points (data and
/// control information) are related by their slot index. The number of slots is configured on construction (start-up of
/// a process).
///
/// This is a data-only class. All behaviour related to EventDataControl is added to EventDataControlLocal.
/// This is because accessing state_slots_ (which is in shared memory) during runtime requires using (dereferencing,
/// copying etc.) OffsetPtrs which can negatively affect performance. Therefore, the data in EventDataControl is created
/// / opened once during Skeleton / Proxy creation, and then is accessed during runtime via ProxyEventDataControlLocal /
/// SkeletonEventDataControlLocal.
///
/// It is one of the corner stone elements of our LoLa IPC for Events!
class EventDataControl
{
  public:
    using EventControlSlots =
        containers::DynamicArray<ControlSlotType, memory::shared::PolymorphicOffsetPtrAllocator<ControlSlotType>>;

    EventDataControl(const SlotIndexType max_slots,
                     score::memory::shared::ManagedMemoryResource& resource,
                     const LolaEventInstanceDeployment::SubscriberCountType max_number_combined_subscribers) noexcept
        : state_slots_{max_slots, resource}, transaction_log_set_{max_number_combined_subscribers, max_slots, resource}
    {
    }

    EventControlSlots state_slots_;
    TransactionLogSet transaction_log_set_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_H
