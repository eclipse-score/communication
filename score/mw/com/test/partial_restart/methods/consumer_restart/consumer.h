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

#include <string_view>

namespace score::mw::com::test
{

/// \brief Consumer actions for consumer restart test.
///        The consumer finds the service, creates a proxy, calls methods to verify they work,
///        then signals checkpoint and waits for the controller to tell it to finish.
void DoConsumerActions(CheckPointControl& check_point_control,
                       score::cpp::stop_token test_stop_token,
                       std::string_view service_instance_manifest) noexcept;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_CONSUMER_RESTART_CONSUMER_H
