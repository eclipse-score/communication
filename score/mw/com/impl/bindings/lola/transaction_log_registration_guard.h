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

#include "score/result/result.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

#include <functional>
#include <optional>

namespace score::mw::com::impl::lola
{

/**
 * RAII helper class that will call TransactionLogSet::RegisterTransactionLog on construction and
 * EventDataControl::UnregisterTransactionLog on destruction.
 */
class TransactionLogRegistrationGuard
{
  public:
    /// \brief Create func for TransactionLogRegistrationGuard for ProxyServiceElementTransactionLog
    /// \param event_data_control event data control for the service element
    /// \param transaction_log_id transaction log is identifying the proxy instance
    /// \return
    static score::Result<TransactionLogRegistrationGuard> Create(EventDataControl& event_data_control,
                                                               const TransactionLogId& transaction_log_id) noexcept;
    /// \brief Create func for TransactionLogRegistrationGuard for SkeletonServiceElementTracingTransactionLog
    /// \param event_data_control event_data_control event data control for the service element
    /// \return
    static TransactionLogRegistrationGuard Create(EventDataControl& event_data_control) noexcept;

    ~TransactionLogRegistrationGuard() noexcept;

    TransactionLogRegistrationGuard(const TransactionLogRegistrationGuard&) = delete;
    TransactionLogRegistrationGuard& operator=(const TransactionLogRegistrationGuard&) = delete;
    TransactionLogRegistrationGuard& operator=(TransactionLogRegistrationGuard&& other) noexcept = delete;

    // The TransactionLogRegistrationGuard must be move constructible so that it can be wrapped in an std::optional.
    TransactionLogRegistrationGuard(TransactionLogRegistrationGuard&& other) noexcept;

    TransactionLogSet::TransactionLogIndex GetTransactionLogIndex() const noexcept;

  private:
    TransactionLogRegistrationGuard(EventDataControl& event_data_control,
                                    const TransactionLogSet::TransactionLogIndex transaction_log_index) noexcept;

    std::reference_wrapper<EventDataControl> event_data_control_;
    std::optional<TransactionLogSet::TransactionLogIndex> transaction_log_index_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_REGISTRATION_GUARD_H
