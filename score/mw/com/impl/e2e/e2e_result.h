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

// Result of the data-integrity check for one sample.
enum class DataIntegrityStatus : std::uint8_t
{
    kDisabled = 0U,
    kOk,
    kError,
};

// Result of the sequence check for one sample.
enum class SequenceStatus : std::uint8_t
{
    kDisabled = 0U,
    kOk,
    kOkGapWithinThreshold,
    kErrorRepeated,
    kErrorGapExceedsThreshold,
};

// Result of the historical health tracking.
enum class HistoricalHealthStatus : std::uint8_t
{
    kDisabled = 0U,
    kOk,
    kError,
};

// Single combined result for applications that do not need the details.
enum class Summary : std::uint8_t
{
    kDisabled = 0U,
    kOk,
    kOkWithDisabledChecks,
    kError,
};

// All E2E results for one sample.
struct E2EResult
{
    DataIntegrityStatus data_integrity{DataIntegrityStatus::kDisabled};
    SequenceStatus sequence{SequenceStatus::kDisabled};
    HistoricalHealthStatus historical_health{HistoricalHealthStatus::kDisabled};
    Summary summary{Summary::kDisabled};
};

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_RESULT_H
