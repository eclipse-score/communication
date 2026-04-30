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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROVIDER_RUNNER_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROVIDER_RUNNER_H
#include "score/mw/com/test/common_test_resources/command_line_parser.h"

#include <score/stop_token.hpp>

#include <cstddef>
#include <vector>

namespace score::mw::com::test
{

void run_provider(const score::cpp::stop_token& stop_token,
                  const std::vector<std::size_t>& expected_method_call_count,
                  std::size_t num_consumers);

auto ParseConsumerRepetitionFromArgs(int argc, const char** argv, std::size_t num_consumers)
    -> std::pair<std::vector<std::size_t>, CommandLineArgsMapType>;

auto CalculateExpectedMethodCallCounts(const std::vector<std::size_t>& consumer_repetition,
                                       std::vector<std::vector<std::size_t>> enabled_method_ids)
    -> std::vector<std::size_t>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROVIDER_RUNNER_H
