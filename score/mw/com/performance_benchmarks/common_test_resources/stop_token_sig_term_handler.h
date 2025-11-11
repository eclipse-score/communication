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
#ifndef SCORE_MW_COM_PERFORMANCE_BENCHMARKS_COMMON_TEST_RESOURCES_SIGTERMHANDLER_H
#define SCORE_MW_COM_PERFORMANCE_BENCHMARKS_COMMON_TEST_RESOURCES_SIGTERMHANDLER_H

#include <score/stop_token.hpp>

namespace score::mw::com
{

bool SetupStopTokenSigTermHandler(score::cpp::stop_source& stop_test);

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_PERFORMANCE_BENCHMARKS_COMMON_TEST_RESOURCES_SIGTERMHANDLER_H
