/*******************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_SERVICE_DISCOVERY_DURING_PROVIDER_CRASH_CONSUMER_H
#define SCORE_MW_COM_TEST_SERVICE_DISCOVERY_DURING_PROVIDER_CRASH_CONSUMER_H

#include "score/mw/com/test/common_test_resources/check_point_control.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <cstdint>
#include <random>

namespace score::mw::com::test
{
inline std::chrono::nanoseconds get_random_time()
{
    auto rd = std::random_device();
    std::uniform_int_distribution<std::uint32_t> dist(0, 501);
    return std::chrono::nanoseconds{dist(rd)};
}

void DoConsumerActionsFirstTime(CheckPointControl& check_point_control,
                                score::cpp::stop_token test_stop_token,
                                int argc,
                                const char** argv);

void DoConsumerActionsAfterRestart(CheckPointControl& check_point_control, int argc, const char** argv);
}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_SERVICE_DISCOVERY_DURING_PROVIDER_CRASH_CONSUMER_H
