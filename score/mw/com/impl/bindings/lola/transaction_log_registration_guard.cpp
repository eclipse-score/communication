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

#include "score/result/result.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::lola
{

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is implicitly called from 'transaction_log_index_result.value()' in case the
// transaction_log_index_result doesn't have value but as we check before with 'has_value()' so no way for throwing
// std::bad_optional_access which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
score::Result<TransactionLogRegistrationGuard> TransactionLogRegistrationGuard::Create(
    ProxyEventDataControlLocalView<>& event_data_control_local,
    const TransactionLogId& transaction_log_id) noexcept
{
    auto& transaction_log_set = event_data_control_local.GetTransactionLogSet();
    auto transaction_log_index_result = transaction_log_set.RegisterProxyElement(transaction_log_id);
    if (!(transaction_log_index_result.has_value()))
    {
        return MakeUnexpected<TransactionLogRegistrationGuard>(transaction_log_index_result.error());
    }
    return TransactionLogRegistrationGuard{transaction_log_set, transaction_log_index_result.value()};
}

TransactionLogRegistrationGuard TransactionLogRegistrationGuard::Create(
    SkeletonEventDataControlLocalView<>& event_data_control_local) noexcept
{
    auto& transaction_log_set = event_data_control_local.GetTransactionLogSet();
    auto transaction_log_index_result = transaction_log_set.RegisterSkeletonTracingElement();
    return TransactionLogRegistrationGuard{transaction_log_set, transaction_log_index_result};
}

TransactionLogRegistrationGuard::TransactionLogRegistrationGuard(
    TransactionLogSet& transaction_log_set,
    const TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept
    : transaction_log_set_{transaction_log_set}, transaction_log_index_{transaction_log_index}
{
}

TransactionLogRegistrationGuard::~TransactionLogRegistrationGuard() noexcept
{
    if (transaction_log_index_.has_value())
    {
        transaction_log_set_.get().Unregister(*transaction_log_index_);
    }
}

TransactionLogRegistrationGuard::TransactionLogRegistrationGuard(TransactionLogRegistrationGuard&& other) noexcept
    // Suppress "AUTOSAR C++14 A18-9-2", The rule states: "Forwarding values to other functions shall be done
    // via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
    // reference. std::forward is already used here.
    // Suppress "AUTOSAR C++14 A12-8-4", The rule states: "Move constructor shall not initialize its class
    // members and base classes using copy semantics".
    // event_data_control_ is a std::reference_wrapper which is a non-owning reference, move operation is no different
    // from copy.
    // transaction_log_index_ is an optional of std::uint8_t which is not move-constructible.
    // coverity[autosar_cpp14_a12_8_4_violation]
    // coverity[autosar_cpp14_a18_9_2_violation]
    : transaction_log_set_{other.transaction_log_set_}, transaction_log_index_{other.transaction_log_index_}
{
    // Suppress "AUTOSAR C++14 A18-9-2" rule findings. This rule stated: "Forwarding values to other functions shall
    // be done via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
    // reference". We are not forwarding any value here, we destroy the inside value.
    // coverity[autosar_cpp14_a18_9_2_violation]
    other.transaction_log_index_.reset();
}

TransactionLogSet::TransactionLogIndex TransactionLogRegistrationGuard::GetTransactionLogIndex() const noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        transaction_log_index_.has_value(), "GetTransactionLogIndex cannot be called without a transaction_log_index");
    return *transaction_log_index_;
}

}  // namespace score::mw::com::impl::lola
