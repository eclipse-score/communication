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

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/proxy_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_index.h"

namespace score::mw::com::impl::lola
{

/// \brief Guard object which will dereference the slot in EventDataControl with the provided index on destruction.
///
/// Since TransactionLogSet::RegisterProxyElement returns a guard object which will call TransactionLogSet::Unregister
/// on destruction, we have to ensure that any slots that are referenced with ReferenceNextEvent() are dereferenced with
/// DereferenceEvent() before Unregister is called.
template <template <class> class AtomicIndirectorType = memory::shared::AtomicIndirectorReal>
class ReferenceGuard
{
  public:
    ReferenceGuard(const SlotIndexType slot_index,
                   ProxyEventDataControlLocalView<AtomicIndirectorType>& proxy_event_data_control_local_view,
                   const TransactionLogIndex transaction_log_index)
        : slot_index_{slot_index},
          proxy_event_data_control_local_view_{proxy_event_data_control_local_view},
          transaction_log_index_{transaction_log_index}
    {
    }

    ~ReferenceGuard()
    {
        proxy_event_data_control_local_view_.DereferenceEvent(slot_index_, transaction_log_index_);
    }

    ReferenceGuard(const ReferenceGuard&) = delete;
    ReferenceGuard& operator=(const ReferenceGuard&) = delete;
    ReferenceGuard(ReferenceGuard&&) noexcept = delete;
    ReferenceGuard& operator=(ReferenceGuard&& other) noexcept = delete;

  private:
    const SlotIndexType slot_index_;
    ProxyEventDataControlLocalView<AtomicIndirectorType>& proxy_event_data_control_local_view_;
    TransactionLogIndex transaction_log_index_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_TEST_RESOURCES_H
