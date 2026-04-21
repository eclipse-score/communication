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
#include "score/mw/com/impl/bindings/lola/transaction_log_registration_guard.h"

#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include <score/assert.hpp>
#include <type_traits>

namespace score::mw::com::impl::lola
{
namespace test
{

void SetDeactiveDestructionOperation(bool deactivate_destruction_operation)
{
    TransactionLogRegistrationGuard::deactivate_destruction_operation_ = deactivate_destruction_operation;
}

}  // namespace test

template <typename T>
using is_movable_or_copyable = std::disjunction<std::is_move_constructible<T>,
                                                std::is_copy_constructible<T>,
                                                std::is_move_assignable<T>,
                                                std::is_copy_assignable<T>>;

bool TransactionLogRegistrationGuard::deactivate_destruction_operation_{false};

TransactionLogRegistrationGuard::TransactionLogRegistrationGuard(
    TransactionLogSet& transaction_log_set,
    const TransactionLogIndex transaction_log_index,
    ConsumerEventDataControlLocalView<>& consumer_event_data_control_local_view)
    : transaction_log_index_{transaction_log_index},
      /// Justification for capturing references without using a ScopedFunction.
      /// Proxy side:
      ///   This function is called by TransactionLogSet::RegisterProxyElement which is called during ProxyEvent
      ///   subscription and destroyed during ProxyEvent unsubscription.
      ///
      ///   The TransactionLogSet resides in shared memory and is created by the Skeleton. Therefore, on the Proxy side,
      ///   the lifetime of the TransactionLogSet starts with Proxy creation and opening the TransactionLogSet in shared
      ///   memory and ends with munmap during Proxy destruction. Therefore, the TransactionLogRegistrationGuard will
      ///   always be destroyed before the TransactionLogSet is destroyed.
      ///
      ///   The ConsumerEventDataControlLocalView is created during ProxyEventCommon creation when opening the shared
      ///   memory created by the Skeleton. It is destroyed during ProxyEvent destruction. Therefore, the
      ///   TransactionLogRegistrationGuard will always be destroyed before the ConsumerEventDataControlLocalView is
      ///   destroyed.
      ///
      /// Skeleton side:
      ///   This function is called when tracing is enabled by TransactionLogSet::RegisterSkeletonTracingElement which
      ///   is called during SkeletonEventCommon::PrepareOfferCommon and destroyed in
      ///   SkeletonEventCommon::PrepareStopOfferCommon.
      ///
      ///   The TransactionLogSet resides in shared memory and is created during Skeleton::PrepareOffer and destroyed
      ///   during Skeleton::PrepareStopOffer. Skeleton::PrepareOffer is always called before
      ///   SkeletonEvent::PrepareOffer and Skeleton::PrepareStopOffer is always called after
      ///   SkeletonEvent::PrepareStopOffer. Therefore, the TransactionLogSet will always be destroyed after the
      ///   TransactionLogRegistrationGuard is destroyed.
      ///
      ///   The ConsumerEventDataControlLocalView is created during Skeleton::Register by SkeletonEventCommon. The
      ///   ConsumerEventDataControlLocalView is owned by the SkeletonEventCommon and destroyed during
      ///   PrepareStopOfferCommon after destroying this guard. Therefore, TransactionLogRegistrationGuard will always
      ///   be destroyed before the ConsumerEventDataControlLocalView is destroyed.
      unregister_on_destruction_operation_{
          [&consumer_event_data_control_local_view, &transaction_log_set, transaction_log_index]() {
              if (!deactivate_destruction_operation_)
              {
                  transaction_log_set.Unregister(transaction_log_index);
                  consumer_event_data_control_local_view.ClearTransactionLogLocalView();
              }
          }}
{
    auto& transaction_log = transaction_log_set.GetTransactionLog(transaction_log_index);
    consumer_event_data_control_local_view.SetTransactionLogLocalView(transaction_log);

    constexpr bool is_transaction_log_set_movable_or_copyable = is_movable_or_copyable<TransactionLogSet>::value;
    static_assert(!is_transaction_log_set_movable_or_copyable,
                  "unregister_on_destruction_operation_ takes a reference to the TransactionLogSet, so we need to make "
                  "sure that TransactionLogSet is not movable or copyable to avoid dangling references.");

    constexpr bool is_consumer_event_data_control_local_view_movable_or_copyable =
        is_movable_or_copyable<ConsumerEventDataControlLocalView<>>::value;
    static_assert(
        !is_consumer_event_data_control_local_view_movable_or_copyable,
        "unregister_on_destruction_operation_ takes a reference to the ConsumerEventDataControlLocalView, so we "
        "need to make sure that it is not movable or copyable to avoid dangling references.");
}

TransactionLogIndex TransactionLogRegistrationGuard::GetTransactionLogIndex() const
{
    return transaction_log_index_;
}

}  // namespace score::mw::com::impl::lola
