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

#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/field_consumer.h"

#include <vector>

int main(int argc, const char** argv)
{
    using Parameters = score::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{
        Parameters::NUM_RETRIES, Parameters::RETRY_BACKOFF_TIME, Parameters::MODE};
    score::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto& run_parameters = test_runner.GetRunParameters();

    return score::mw::com::test::run_client(
        run_parameters.GetNumRetries(), run_parameters.GetRetryBackoffTime(), run_parameters.GetMode());
}
