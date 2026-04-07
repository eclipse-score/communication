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
#ifndef SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_CONSUMER_RESTART_CONTROLLER_H
#define SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_CONSUMER_RESTART_CONTROLLER_H

#include <score/stop_token.hpp>

namespace score::mw::com::test
{

/// \brief Test variant 1: Consumer graceful/normal restart.
///        Provider stays up the entire time. Consumer is started, calls methods successfully,
///        then exits gracefully. A new consumer is started and verifies it can find the service
///        and call methods successfully again.
int DoConsumerNormalRestart(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept;

/// \brief Test variant 2: Consumer kill/crash restart.
///        Provider stays up the entire time. Consumer is started, calls methods successfully,
///        then is killed via SIGKILL. A new consumer is started and verifies it can find the service
///        and call methods successfully again.
int DoConsumerKillRestart(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_CONSUMER_RESTART_CONTROLLER_H
