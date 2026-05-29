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

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_FIELD_PROVIDER_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_FIELD_PROVIDER_H

#include <score/stop_token.hpp>

#include <chrono>
#include <string>

namespace score::mw::com::test
{

int run_service(const std::chrono::milliseconds& cycle_time,
                const score::cpp::stop_token& stop_token,
                const std::string& mode);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_FIELD_PROVIDER_H
