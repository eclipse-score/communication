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

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"
#include "score/mw/com/test/methods/methods_test_resources/consumer_runner.h"
#include "score/mw/com/test/methods/methods_test_resources/provider_runner.h"

#include <iostream>

#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"
#include <score/jthread.hpp>
#include <score/stop_token.hpp>

namespace score::mw::com::test
{
namespace
{

struct CombinedTestConfiguration
{
    std::vector<std::size_t> consumer_repetition;
    std::string service_instance_manifest;
};
auto ReadCombinedCommandLineArguments(int argc, const char** argv, std::size_t num_consumers)
    -> CombinedTestConfiguration
{
    auto [consumer_repetition, args] = ParseConsumerRepetitionFromArgs(argc, argv, num_consumers);

    std::string defuault_pat{"./etc/mw_com_config.json"};
    return CombinedTestConfiguration{
        consumer_repetition, GetValueIfProvided<std::string>(args, "service-instance-manifest").value_or(defuault_pat)};
}

}  // namespace
}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    auto num_consumers = score::mw::com::test::kConfiguredNumberOfConsumers;

    score::mw::com::test::CombinedTestConfiguration combined_test_configuration{
        score::mw::com::test::ReadCombinedCommandLineArguments(argc, argv, num_consumers)};
    auto expected_method_call_count = score::mw::com::test::CalculateExpectedMethodCallCounts(
        combined_test_configuration.consumer_repetition, score::mw::com::test::kEnabledMethodIDs);

    std::cout << "service-instance-manifest: " << combined_test_configuration.service_instance_manifest << "\n";

    score::mw::com::test::InitializeRuntime(combined_test_configuration.service_instance_manifest);

    // ------ Start the Provider Threads
    std::vector<score::cpp::jthread> consumer_threads;

    for (std::size_t consumer_id{0}; consumer_id < num_consumers; ++consumer_id)
    {
        score::mw::com::test::ConsumerTestConfiguration consumer_test_configuration{};
        consumer_test_configuration.consumer_id = consumer_id;
        consumer_test_configuration.num_proxies_per_process =
            combined_test_configuration.consumer_repetition.at(consumer_id);
        consumer_test_configuration.service_instance_manifest = combined_test_configuration.service_instance_manifest;
        auto enabled_method_ids = score::mw::com::test::kEnabledMethodIDs.at(consumer_id);
        consumer_threads.emplace_back(
            score::mw::com::test::run_consumer, consumer_test_configuration, enabled_method_ids);
    }

    // ------ Start the consumer

    score::cpp::stop_source stop_source_{};
    score::mw::com::test::run_provider(stop_source_.get_token(), expected_method_call_count, num_consumers);

    return EXIT_SUCCESS;
}
