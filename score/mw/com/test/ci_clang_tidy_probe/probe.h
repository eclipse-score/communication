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
#ifndef SCORE_MW_COM_TEST_CI_CLANG_TIDY_PROBE_PROBE_H
#define SCORE_MW_COM_TEST_CI_CLANG_TIDY_PROBE_PROBE_H

namespace score::mw::com::test::ci_clang_tidy_probe
{

// This function only exists to intentionally trigger a clang-tidy finding
// (readability-magic-numbers) so that CI linting behavior can be validated.
// Remove this whole directory once the validation is done.
int AddMagicOffset(int value);

}  // namespace score::mw::com::test::ci_clang_tidy_probe

#endif  // SCORE_MW_COM_TEST_CI_CLANG_TIDY_PROBE_PROBE_H
