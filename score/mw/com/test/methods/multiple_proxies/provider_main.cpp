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
#include "score/mw/com/test/methods/methods_test_resources/provider_runner.h"
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"

#include <score/stop_token.hpp>
#include <cstddef>
#include <cstdlib>

int main(int argc, const char** argv)
{
    const std::size_t num_consumers = score::mw::com::test::kConfiguredNumberOfConsumers;
    auto [consumer_repetition, args] = score::mw::com::test::ParseConsumerRepetitionFromArgs(argc, argv, num_consumers);
    auto expected_method_call_count = score::mw::com::test::CalculateExpectedMethodCallCounts(
        consumer_repetition, score::mw::com::test::kEnabledMethodIDs);

    score::cpp::stop_source stop_source_{};
    score::mw::com::test::run_provider(stop_source_.get_token(), expected_method_call_count, num_consumers);
    return EXIT_SUCCESS;
}
