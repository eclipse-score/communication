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
#ifndef SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_CONSUMER_RESTART_CONSUMER_H
#define SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_CONSUMER_RESTART_CONSUMER_H

#include "score/mw/com/test/common_test_resources/check_point_control.h"

#include <score/stop_token.hpp>

namespace score::mw::com::test
{

/// \brief Consumer actions for consumer restart test.
///        The consumer finds the service, creates a proxy, calls methods to verify they work,
///        then signals checkpoint and waits for the controller to tell it to finish.
///
/// \param enable_methods If true, creates proxy with method names and calls methods.
///        If false, creates proxy WITHOUT methods (just verifies FindService + proxy creation work).
///        The second consumer after restart must use false because the current LoLa implementation
///        does not clean up method shared memory when a consumer exits, so a new consumer
///        cannot re-create the method shared memory.
void DoConsumerActions(CheckPointControl& check_point_control,
                       score::cpp::stop_token test_stop_token,
                       int argc,
                       const char** argv,
                       bool enable_methods) noexcept;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_CONSUMER_RESTART_CONSUMER_H
