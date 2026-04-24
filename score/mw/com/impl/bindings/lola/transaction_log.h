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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_H

#include "score/mw/com/impl/bindings/lola/transaction_log_slot.h"

#include "score/containers/dynamic_array.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <cstdint>

namespace score::mw::com::impl::lola
{

/// \brief Class which contains the state of a Proxy service element's (i.e. ProxyEvent / ProxyField) interaction with
///        shared memory.
///
/// Each Proxy service element instance will have its own TransactionLog which will record any Subscribe / Unsubscribe
/// calls as well as increments / decrements to the reference count of the corresponding Skeleton service element. The
/// TransactionLog has a Rollback function which undoes any previous operations that were recorded in the TransactionLog
/// so that the service element can be recreated (e.g. in the case of a crash).
class TransactionLog
{
  public:
    /// \todo Add a central location in which all the type aliases are placed so that these types align with their
    /// usages in other parts of the code base.
    using SlotIndexType = std::uint16_t;
    using MaxSampleCountType = std::uint16_t;

    using TransactionLogSlots =
        score::containers::DynamicArray<TransactionLogSlot,
                                        memory::shared::PolymorphicOffsetPtrAllocator<TransactionLogSlot>>;

    TransactionLog(const std::size_t number_of_slots, memory::shared::ManagedMemoryResource& resource) noexcept;

    /// \brief Vector containing one TransactionLogSlot for each slot in the corresponding control vector.
    TransactionLogSlots reference_count_slots_;

    /// \brief TransactionLogSlot in shared memory which will record subscribe / unsubscribe transactions.
    TransactionLogSlot subscribe_transactions_;

    /// \brief The max sample count used for the recorded subscription transaction.
    ///
    /// This is set in SubscribeTransactionBegin() and used in the UnsubscribeCallback which is called during Rollback()
    score::cpp::optional<MaxSampleCountType> subscription_max_sample_count_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_H
