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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_ID_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_ID_H

#include <sys/types.h>

#include <cstdint>
#include <limits>

namespace score::mw::com::impl::lola
{

/// \brief A unique identifier for identifying / retrieving a TransactionLog
/// \details The TransactionLogId is needed so that a Proxy / Skeleton service element can retrieve its own
/// TransactionLog after a crash. It is either the explicitly configured applicationID or falls back to the process uid.
/// Note: If multiple Proxy objects to the same service are created within a single process, they will all share this
/// same identifier. Similarly, 2 instantiations of the same ProxyEvent will share the same TransactionLogId. This is
/// acceptable since ALL service elements within a process will live / die together. So in the TransactionLogSet
/// rollback mechanism, we can simply rollback all TransactionLogs corresponding to a given TransactionLogId.
using TransactionLogId = uid_t;

/// \brief We assign max TransactionLogId as the "invalid"/initial TransactionLogId our initially created transaction
///        logs will have. Since we expect, that no process will run with this max-uid. We assert if this would be the
///        case. Note, that using the min-uid (0) is NOT an option, since this uid is taken regularly, i.e. in case of
///        SCTF tests
/// \details We are asserting in TransactionLogSet::TransactionLogNode::TryAcquire, that this API doesn't get called
///          with kInvalidTransactionLogId!
/// \details This TransactionLogSet is shared across processes/toolchains
///          (e.g. between the QNX Domain and Linux/Android Lola Domain sides of the mw-com gateway),
///          so this sentinel must resolve to the identical bit pattern everywhere it's placed
///          in shared memory. Deriving it from std::numeric_limits<TransactionLogId>::max() is NOT safe for that,
///          because uid_t's signedness differs by platform (signed 32-bit on QNX vs unsigned 32-bit on Linux/Android
///          bionic), so std::numeric_limits<uid_t>::max() silently resolves to a different value (2147483647 vs
///          4294967295) on each side even though the source is identical. All real TransactionLogIds come from
///          GlobalConfiguration::ApplicationId (std::uint32_t) or a process uid cast to std::uint32_t, so
///          INT32_MAX is used directly as a fixed, platform-independent sentinel that both sides agree on.
// Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
// variables being given values that are not subsequently used".
// This constant definition is used by other units to represent an invalid/initial TransactionLogId.
// Suppress "AUTOSAR C++14 A2-10-4". This rule states: "The identifier name of a non-member object with static storage
// duration or static function shall not be reused within a namespace.".
// This variable is declared only once within this namespace and does not violate the rule.
// coverity[autosar_cpp14_a2_10_4_violation]
// coverity[autosar_cpp14_a0_1_1_violation]
static_assert(std::numeric_limits<std::int32_t>::max() <= std::numeric_limits<TransactionLogId>::max(),
              "TransactionLogId must represent INT32_MAX");
constexpr TransactionLogId kInvalidTransactionLogId{
    static_cast<TransactionLogId>(std::numeric_limits<std::int32_t>::max())};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_ID_H
