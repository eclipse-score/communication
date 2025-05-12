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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_ROLLBACK_EXECUTOR_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_ROLLBACK_EXECUTOR_H

#include "score/mw/com/impl/bindings/lola/runtime.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"

namespace score::mw::com::impl::lola
{

class TransactionLogRollbackExecutor
{
  public:
    /// \brief ctor of proxy instance specific TransactionLogRollbackExecutor
    /// \param service_data_control pointer to service instance specific control within shared-mem.
    /// \param asil_level asil level of the proxy instance owning this executor.
    /// \param provider_pid pid/node-id of the service instance provider
    /// \param transaction_log_id id of transaction logs to be rolled back.
    TransactionLogRollbackExecutor(ServiceDataControl& service_data_control,
                                   const QualityType asil_level,
                                   pid_t provider_pid,
                                   const TransactionLogId transaction_log_id) noexcept;

    /// \brief Does a rollback of all transaction logs (log per service element) related to service_data_control/
    ///        transaction_log_id specific to a proxy instance given in the ctor.
    /// \details Besides the pure transaction rollback, there is also some preparation needed/done once for a given
    ///          service_data_control (independent from the number of local proxy instances referring to it). This is
    ///          done by an internal call to #PrepareRollback
    ResultBlank RollbackTransactionLogs() noexcept;

  private:
    /// \brief Prepares the rollback of proxy service instance specific transaction logs.
    /// \details This "preparation" must only be done ONCE in the context of a "proxy process", which accesses the
    ///          related service-instance! The implementation cares for this and returns early, if preparation has
    ///          already been done. I.e. in the pathological case, that a process has multiple proxy instances
    ///          for the same service-instance, only the call to this func by the 1st proxy instance will do the
    ///          preparation, further calls will return early.
    ///          The func does mainly two things:
    ///          1. It registers current process uid/pid pair within service_data_control and if it detects an old/
    ///             previous registration for its uid, it notifies the provider side about an outdated pid.
    ///          2. It marks any transaction-log within service_data_control with its transaction_log_id as
    ///             "to-be-rolled-back".
    void PrepareRollback(lola::IRuntime& lola_runtime) noexcept;

    ServiceDataControl& service_data_control_;
    /// asil level of the service_data_control_
    const QualityType asil_level_;
    /// pid of the provider of the service instance represented by service_data_control_
    const pid_t provider_pid_;
    TransactionLogId transaction_log_id_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_ROLLBACK_EXECUTOR_H
