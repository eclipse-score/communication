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

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_FIELD_CONSUMER_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_FIELD_CONSUMER_H

#include <chrono>
#include <cstddef>
#include <string>

namespace score::mw::com::test
{

void run_consumer(std::size_t num_retries, std::chrono::milliseconds retry_backoff_time, const std::string& mode);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_FIELD_CONSUMER_H
