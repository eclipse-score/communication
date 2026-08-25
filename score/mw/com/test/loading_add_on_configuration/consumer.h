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

#ifndef SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_CONSUMER_H
#define SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_CONSUMER_H

#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

namespace score::mw::com::test
{
void run_consumer(const score::cpp::stop_token& stop_token,
                  const score::mw::com::InstanceSpecifier& instance_specifier,
                  ProcessSynchronizer& process_synchronizer,
                  ProcessSynchronizer& provider_ready_synchronizer,
                  const std::vector<std::uint32_t>& samples);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_CONSUMER_H
