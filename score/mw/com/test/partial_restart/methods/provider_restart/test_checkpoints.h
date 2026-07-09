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
#ifndef SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_PROVIDER_RESTART_TEST_CHECKPOINTS_H
#define SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_PROVIDER_RESTART_TEST_CHECKPOINTS_H

#include <cstdint>

namespace score::mw::com::test
{

constexpr std::uint8_t kCheckpointMethodCallsSucceeded{1U};
constexpr std::uint8_t kCheckpointServiceStopped{2U};
constexpr std::uint8_t kCheckpointServiceReappeared{3U};
constexpr std::uint8_t kCheckpointMethodsCalledAfterRestart{4U};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_PROVIDER_RESTART_TEST_CHECKPOINTS_H
