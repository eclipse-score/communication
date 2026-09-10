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
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"
#include "score/mw/com/test/methods/multiple_proxies/consumer.h"
#include "score/mw/com/test/methods/multiple_proxies/provider.h"

#include <score/jthread.hpp>
#include <score/stop_token.hpp>
#include <future>

namespace score::mw::com::test
{
namespace
{

const std::string kNumConsumers{"num-consumers"};
const std::string kNumProxiesPerConsumerKey{"num-proxies-per-consumer"};
const std::string kNumMethodCallsPerProxyKey{"num-method-calls-per-proxy"};
const std::string kServiceInstanceManifest{"service-instance-manifest"};

struct CombinedTestConfiguration
{
    std::size_t num_consumers;
    std::size_t num_proxies_per_process;
    std::size_t num_method_calls_per_proxy;
    std::string service_instance_manifest;
};

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv)
{
    auto args = ParseCommandLineArguments(argc,
                                          argv,
                                          {{kNumConsumers, ""},
                                           {kNumProxiesPerConsumerKey, ""},
                                           {kNumMethodCallsPerProxyKey, ""},
                                           {kServiceInstanceManifest, ""}});

    const auto num_consumers = GetValue<std::size_t>(args, kNumConsumers);
    if (num_consumers <= 0)
    {
        FailTest("Consumer: ", kNumConsumers, " value ", num_consumers, " must be greater than 0.");
    }

    const auto num_proxies_per_process = GetValue<std::size_t>(args, kNumProxiesPerConsumerKey);
    if (num_proxies_per_process <= 0)
    {
        FailTest(
            "Consumer: ", kNumProxiesPerConsumerKey, " value ", num_proxies_per_process, " must be greater than 0.");
    }

    const auto num_method_calls_per_proxy = GetValue<std::size_t>(args, kNumMethodCallsPerProxyKey);
    if (num_method_calls_per_proxy <= 0)
    {
        FailTest("Consumer: ",
                 kNumMethodCallsPerProxyKey,
                 " value ",
                 num_method_calls_per_proxy,
                 " must be greater than 0.");
    }

    auto service_instance_manifest = GetValue<std::string>(args, kServiceInstanceManifest);

    return {num_consumers, num_proxies_per_process, num_method_calls_per_proxy, std::move(service_instance_manifest)};
}

}  // namespace
}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    auto test_configuration{score::mw::com::test::ReadCommandLineArguments(argc, argv)};
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{test_configuration.service_instance_manifest});

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    // ------ Start the Consumer Threads
    std::vector<std::future<void>> consumer_futures{};

    for (std::size_t consumer_id{0}; consumer_id < test_configuration.num_consumers; ++consumer_id)
    {
        const auto& enabled_method_ids = score::mw::com::test::kEnabledMethodsPerProxy.at(consumer_id);
        auto consumer_future = std::async(std::launch::async,
                                          score::mw::com::test::run_consumer,
                                          consumer_id,
                                          test_configuration.num_proxies_per_process,
                                          test_configuration.num_method_calls_per_proxy,
                                          enabled_method_ids);
        consumer_futures.push_back(std::move(consumer_future));
    }

    // ------ Start the provider
    score::mw::com::test::run_provider(stop_source.get_token(),
                                       test_configuration.num_consumers,
                                       test_configuration.num_proxies_per_process,
                                       test_configuration.num_method_calls_per_proxy);

    for (auto& consumer_future : consumer_futures)
    {
        consumer_future.get();
    }
    return EXIT_SUCCESS;
}
