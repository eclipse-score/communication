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

#ifndef SCORE_MW_COM_TEST_PARTIAL_RESTART_PROXY_RESTART_SHALL_NOT_AFFECT_OTHER_PROXIES_CONSUMER_H
#define SCORE_MW_COM_TEST_PARTIAL_RESTART_PROXY_RESTART_SHALL_NOT_AFFECT_OTHER_PROXIES_CONSUMER_H
#include "score/mw/com/test/common_test_resources/check_point_control.h"

namespace score::mw::com::test
{
void PerformFirstConsumerActions(CheckPointControl& check_point_control, score::cpp::stop_token stop_token);

void PerformSecondConsumerActions(CheckPointControl& check_point_control,
                                  score::cpp::stop_token stop_token,
                                  const size_t create_proxy_and_receive_M_times);
}  // namespace score::mw::com::test
#endif  // SCORE_MW_COM_TEST_PARTIAL_RESTART_PROXY_RESTART_SHALL_NOT_AFFECT_OTHER_PROXIES_CONSUMER_H
