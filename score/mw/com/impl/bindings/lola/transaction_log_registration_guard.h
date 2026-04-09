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

#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_index.h"

#include "score/scope_exit/scope_exit.h"

#include <score/callback.hpp>

namespace score::mw::com::impl::lola
{
namespace test
{

void SetDeactiveDestructionOperation(bool deactivate_destruction_operation);

}

class TransactionLogSet;

/// \brief RAII helper class that will inject the TransactionLogLocalView of the registered TransactionLog (that was
/// just registered in the TransactionLogSet) in the ConsumerEventDataControlLocalView and will unset the
/// TransactionLogLocalView and call TransactionLogSet::Unregister on destruction.
///
/// In theory, the injection of the cached TransactionLogLocalView in the ConsumerEventDataControlLocalView could also
/// have been done by the TransactionLogSet. We make this class responsible for both injecting and destroying the cached
/// TransactionLogLocalView to keep the injection / destruction logic in one class. Furthermore, those functions are
/// private and this class is made a friend of ConsumerEventDataControlLocalView to ensure that noone else can affect
/// the lifetime of the cached TransactionLogLocalView. This way, TransactionLogSet does not also have to be made a
/// friend of ConsumerEventDataControlLocalView.
///
/// Class must not be destroyed concurrently with a call to TransactionLogSet::GetTransactionLog with the same
/// transaction_log_index.
class TransactionLogRegistrationGuard
{
    // Suppress "AUTOSAR C++14 A11-3-1"
    // Design dessision: The "*Attorney" class is a helper, which reads can access private members of this class and
    // used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class TransactionLogRegistrationGuardTestAttorney;

    friend void test::SetDeactiveDestructionOperation(bool deactivate_destruction_operation);

  public:
    TransactionLogRegistrationGuard(TransactionLogSet& transaction_log_set,
                                    const TransactionLogIndex transaction_log_index,
                                    ConsumerEventDataControlLocalView<>& consumer_event_control_local_view_variant);

    [[nodiscard]] TransactionLogIndex GetTransactionLogIndex() const;

  private:
    TransactionLogIndex transaction_log_index_;

    // Calls registered handler on destruction. Justification for why we use a score::cpp::callback instead of a scoped
    // function is above the instantiation of this object in the constructor.
    utils::ScopeExit<score::cpp::callback<void()>> unregister_on_destruction_operation_;
    static_assert(
        std::is_same_v<decltype(unregister_on_destruction_operation_), utils::ScopeExit<score::cpp::callback<void()>>>,
        "unregister_on_destruction_operation_ should be of type "
        "utils::MovableScopedOperation<score::cpp::callback<void()>> since we rely on testing done in "
        "MoveableScopedOperation. If it changes types, tests must be added for move operations.");

    /// \brief Test-only flag which if false, prevents unregister_on_destruction_operation_ being called on destruction.
    ///
    /// In production code, a rollback will only be done in a restart case on the TransactionLog which is opened in
    /// shared memory which was created by the crashed process. If the process crashed, then any
    /// TransactionLogRegistrationGuards that were created would no longer exist. Therefore, we will never have a
    /// rollback and unregister_on_destruction_operation_ for the same TransactionLog called within a single process.
    /// However, in tests, we always return a TransactionLogRegistrationGuard from RegisterProxyElement /
    /// RegisterSkeletonTracingElement. Therefore, even in tests in which we call rollback, there will still be a valid
    /// TransactionLogRegistrationGuard which must be destroyed. When this happens, a check in the
    /// TransactionLogNode::Release function will fail since the TransactionLogNode would have already been rolled back
    /// and thus marked as inactive. Therefore, in these specific tests, we should set deactivate_destruction_operation_
    /// to true.
    static bool deactivate_destruction_operation_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_REGISTRATION_GUARD_H
