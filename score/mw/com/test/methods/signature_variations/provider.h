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
#ifndef SCORE_MW_COM_TEST_METHODS_SIGNATURE_VARIATIONS_PROVIDER_H
#define SCORE_MW_COM_TEST_METHODS_SIGNATURE_VARIATIONS_PROVIDER_H

#include <score/stop_token.hpp>

namespace score::mw::com::test
{

bool run_provider(const score::cpp::stop_token& stop_token);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_SIGNATURE_VARIATIONS_PROVIDER_H
