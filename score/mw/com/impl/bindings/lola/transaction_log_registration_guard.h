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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_REGISTRATION_GUARD_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_REGISTRATION_GUARD_H

#include "score/mw/com/impl/bindings/lola/transaction_log_index.h"

#include "score/utils/movable_scoped_operation.h"

namespace score::mw::com::impl::lola
{

class TransactionLogSet;

/// \brief RAII helper class that will call TransactionLogSet::Unregister on destruction.
///
/// Class must not be destroyed concurrently with a call to TransactionLogSet::GetTransactionLog with the same
/// transaction_log_index.
class TransactionLogRegistrationGuard
{
    // Suppress "AUTOSAR C++14 A11-3-1"
    // Design dessision: The "*Attorney" class is a helper, which reads can access private members of this class and
    // used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class TransactionLogRegistrationGuardAttorney;

  public:
    TransactionLogRegistrationGuard(TransactionLogSet& transaction_log_set,
                                    const TransactionLogIndex transaction_log_index);

    TransactionLogIndex GetTransactionLogIndex() const;

  private:
    TransactionLogIndex transaction_log_index_;

    /// The lifetime of the TransactionLogSet starts with Proxy creation and opening the TrasnactionlogSet in shared
    /// memory and ends with munmap during Proxy destruction. The TransactionLogRegistrationGuard is held
    /// by the SubscriptionStateMachine which is owned by the ProxyEvent which is owned by the Proxy. Therefore, the
    /// TransactionLogRegistrationGuard will always be destroyed before the TransactionLogSet is destroyed, so it is
    /// safe to capture a reference to the TransactionLogSet in the lambda without using a ScopedFunction.
    utils::MovableScopedOperation<score::cpp::callback<void()>> unregister_on_destruction_operation_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_REGISTRATION_GUARD_H
