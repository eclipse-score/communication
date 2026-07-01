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
#ifndef SCORE_MW_COM_TEST_PROXY_EVENT_MOVE_SEMANTICS_PROVIDER_H
#define SCORE_MW_COM_TEST_PROXY_EVENT_MOVE_SEMANTICS_PROVIDER_H

#include "score/mw/com/test/move_semantics/proxy_event/test_parameters.h"

#include <score/stop_token.hpp>

namespace score::mw::com::test
{

void RunProvider(const ProxyMoveScenario& scenario,
                 const std::size_t num_samples_to_send,
                 const score::cpp::stop_token& stop_token);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PROXY_EVENT_MOVE_SEMANTICS_PROVIDER_H
