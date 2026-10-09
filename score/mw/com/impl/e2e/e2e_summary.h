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
#ifndef SCORE_MW_COM_IMPL_E2E_E2E_SUMMARY_H
#define SCORE_MW_COM_IMPL_E2E_E2E_SUMMARY_H

#include "score/mw/com/impl/e2e/e2e_health_tracker.h"
#include "score/mw/com/impl/e2e/e2e_result.h"

namespace score::mw::com::impl::e2e
{

// Combines the three results into one Summary.
// Order: any error -> kError, all disabled -> kDisabled, some disabled -> kOkWithDisabledChecks, else kOk.
Summary ComputeSummary(DataIntegrityStatus data_integrity,
                       SequenceStatus sequence,
                       HistoricalHealthStatus historical_health) noexcept;

// Updates the historical health, computes the Summary and returns the full result for one sample.
E2EResult BuildE2EResult(DataIntegrityStatus data_integrity,
                         SequenceStatus sequence,
                         HealthTracker& health_tracker) noexcept;

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_SUMMARY_H
