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
#include "score/mw/com/test/methods/multiple_proxies/provider.h"

#include <score/stop_token.hpp>

#include <cstddef>
#include <cstdlib>

namespace score::mw::com::test
{

const std::string kNumConsumers{"num-consumers"};
const std::string kNumProxiesPerConsumerKey{"num-proxies-per-consumer"};
const std::string kNumMethodCallsPerProxyKey{"num-method-calls-per-proxy"};
const std::string kServiceInstanceManifest{"service-instance-manifest"};

struct ProviderTestConfiguration
{
    std::size_t num_consumers;
    std::size_t num_proxies_per_process;
    std::size_t num_method_calls_per_proxy;
    std::string service_instance_manifest;
};

ProviderTestConfiguration ReadCommandLineArguments(int argc, const char** argv)
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
    if (num_proxies_per_process <= 0)
    {
        FailTest(
            "Consumer: ", kNumMethodCallsPerProxyKey, " value ", num_proxies_per_process, " must be greater than 0.");
    }

    auto service_instance_manifest = GetValue<std::string>(args, kServiceInstanceManifest);

    return {num_consumers, num_proxies_per_process, num_method_calls_per_proxy, std::move(service_instance_manifest)};
}

}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    const auto test_configuration = score::mw::com::test::ReadCommandLineArguments(argc, argv);
    const auto expected_num_consumers = score::mw::com::test::kEnabledMethodsPerProxy.size();
    if (test_configuration.num_consumers > expected_num_consumers)
    {
        score::mw::com::test::FailTest("Provider: num_consumers ",
                                       test_configuration.num_consumers,
                                       " must be smaller than the size of kEnabledMethodsPerProxy which is currently ",
                                       expected_num_consumers);
    }

    auto expected_method_call_count =
        score::mw::com::test::CalculateExpectedMethodCallCounts(test_configuration.num_consumers,
                                                                test_configuration.num_proxies_per_process,
                                                                test_configuration.num_method_calls_per_proxy,
                                                                score::mw::com::test::kEnabledMethodsPerProxy);
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{test_configuration.service_instance_manifest});

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    score::mw::com::test::run_provider(
        stop_source.get_token(), expected_method_call_count, test_configuration.num_consumers);
    return EXIT_SUCCESS;
}
