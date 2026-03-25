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

#include "score/result/result.h"

#include <score/assert.hpp>
#include <type_traits>

namespace score::mw::com::impl::lola
{

TransactionLogRegistrationGuard::TransactionLogRegistrationGuard(TransactionLogSet& transaction_log_set,
                                                                 const TransactionLogIndex transaction_log_index)
    : transaction_log_index_{transaction_log_index},
      unregister_on_destruction_operation_{[&transaction_log_set, transaction_log_index]() {
          transaction_log_set.Unregister(transaction_log_index);
      }}
{
    constexpr bool is_transaction_log_set_movable_or_copyable =
        std::is_move_constructible_v<TransactionLogSet> || std::is_copy_constructible_v<TransactionLogSet> ||
        std::is_move_assignable_v<TransactionLogSet> || std::is_copy_assignable_v<TransactionLogSet>;
    static_assert(!is_transaction_log_set_movable_or_copyable,
                  "unregister_on_destruction_operation_ takes a reference to the TransactionLogSet, so we need to make "
                  "sure that TransactionLogSet is not movable or copyable to avoid dangling references.");
}

TransactionLogIndex TransactionLogRegistrationGuard::GetTransactionLogIndex() const
{
    return transaction_log_index_;
}

}  // namespace score::mw::com::impl::lola
