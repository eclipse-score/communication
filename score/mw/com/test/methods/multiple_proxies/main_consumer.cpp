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
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"
#include "score/mw/com/test/methods/multiple_proxies/consumer.h"

namespace score::mw::com::test
{

const std::string kConsumerIdKey{"consumer-id"};
const std::string kNumProxiesPerConsumerKey{"num-proxies-per-consumer"};
const std::string kNumMethodCallsPerProxyKey{"num-method-calls-per-proxy"};
const std::string kServiceInstanceManifest{"service-instance-manifest"};

struct ConsumerTestConfiguration
{
    std::size_t consumer_id;
    std::size_t num_proxies_per_process;
    std::size_t num_method_calls_per_proxy;
    std::string service_instance_manifest;
};

ConsumerTestConfiguration ReadCommandLineArguments(int argc, const char** argv)
{
    auto args = ParseCommandLineArguments(argc,
                                          argv,
                                          {{kConsumerIdKey, ""},
                                           {kNumProxiesPerConsumerKey, ""},
                                           {kNumMethodCallsPerProxyKey, ""},
                                           {kServiceInstanceManifest, ""}});

    const auto consumer_id = GetValue<std::size_t>(args, kConsumerIdKey);

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

    return {consumer_id, num_proxies_per_process, num_method_calls_per_proxy, std::move(service_instance_manifest)};
}

}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    const auto test_configuration = score::mw::com::test::ReadCommandLineArguments(argc, argv);

    const auto& enabled_method_ids = score::mw::com::test::kEnabledMethodsPerProxy.at(test_configuration.consumer_id);
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{test_configuration.service_instance_manifest});
    score::mw::com::test::run_consumer(test_configuration.consumer_id,
                                       test_configuration.num_proxies_per_process,
                                       test_configuration.num_method_calls_per_proxy,
                                       enabled_method_ids);
    return EXIT_SUCCESS;
}
