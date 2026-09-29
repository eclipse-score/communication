/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_E2E_E2E_RESULT_H
#define SCORE_MW_COM_IMPL_E2E_E2E_RESULT_H

#include <cstdint>

namespace score::mw::com::impl::e2e
{

/// \brief Result of a data-integrity check for one received sample.
enum class DataIntegrityStatus : std::uint8_t
{
    kDisabled,
    kOk,
    kError,
};

/// \brief Result of a sequence check for one received sample.
enum class SequenceStatus : std::uint8_t
{
    kDisabled,
    kOk,
    kOkGapWithinThreshold,
    kErrorRepeated,
    kErrorGapExceedsThreshold,
};

/// \brief Result of binding-independent historical E2E health tracking.
enum class HistoricalHealthStatus : std::uint8_t
{
    kDisabled,
    kOk,
    kError,
};

/// \brief Aggregated result for applications that do not need E2E detail.
enum class Summary : std::uint8_t
{
    kDisabled,
    kOk,
    kOkWithDisabledChecks,
    kError,
};

/// \brief Complete E2E result associated with one received sample.
struct E2EResult
{
    DataIntegrityStatus data_integrity{DataIntegrityStatus::kDisabled};
    SequenceStatus sequence{SequenceStatus::kDisabled};
    HistoricalHealthStatus historical_health{HistoricalHealthStatus::kDisabled};
    Summary summary{Summary::kDisabled};
};

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_RESULT_H
