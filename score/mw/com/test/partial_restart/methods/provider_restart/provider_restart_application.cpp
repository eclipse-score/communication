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
#include "score/mw/com/test/partial_restart/methods/provider_restart/provider_restart_application.h"
#include "score/mw/com/test/partial_restart/methods/provider_restart/controller.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace
{

/// \brief Test parameters for the ITF test variants for provider restart with methods.
/// \details We have four variants for provider restart ITF with methods. This is reflected
///          in the test parameters with_proxy and kill_provider:
///          ITF variant 1: Provider graceful/normal restart, while having a consumer with proxy
///                         -> with_proxy{true}, kill_provider{false}
///          ITF variant 2: Provider graceful/normal restart, without proxy
///                         -> with_proxy{false}, kill_provider{false}
///          ITF variant 3: Provider kill/crash restart, while having a consumer with proxy
///                         -> with_proxy{true}, kill_provider{true}
///          ITF variant 4: Provider kill/crash restart, without proxy
///                         -> with_proxy{false}, kill_provider{true}
struct TestParameters
{
    std::string service_instance_manifest{};
    std::size_t number_test_iterations{};
    /// \brief shall a proxy be created on consumer side (which then also tests implicitly proxy-auto-reconnect)
    bool with_proxy{true};
    /// \brief shall the provider be killed (true) or gracefully shutdown (false) before restart.
    bool kill_provider{false};
};

const std::string kServiceInstanceManifestArg{"service_instance_manifest"};
const std::string kTurnsArg{"turns"};
const std::string kKillArg{"kill"};
const std::string kWithProxyArg{"with-proxy"};

std::optional<TestParameters> ReadCommandLineArguments(int argc, const char** argv) noexcept
{
    TestParameters test_parameters{};

    const auto args = score::mw::com::test::ParseCommandLineArguments(
        argc,
        argv,
        {{kServiceInstanceManifestArg, "path to the com configuration file"},
         {kTurnsArg, "number of cycles (provider restarts) to be done"},
         {kKillArg, "whether to kill provider before restart"},
         {kWithProxyArg, "whether to create a proxy instance"}});

    const auto number_test_iterations_result = score::mw::com::test::GetValueIfProvided<std::size_t>(args, kTurnsArg);
    if (!number_test_iterations_result.has_value())
    {
        std::cout << "Test main: Missing or invalid '--" << kTurnsArg
                  << "' argument: " << number_test_iterations_result.error().Message() << std::endl;
        return {};
    }
    test_parameters.number_test_iterations = number_test_iterations_result.value();

    const auto kill_provider_result = score::mw::com::test::GetValueIfProvided<bool>(args, kKillArg);
    if (kill_provider_result.has_value())
    {
        test_parameters.kill_provider = kill_provider_result.value();
    }

    const auto with_proxy_result = score::mw::com::test::GetValueIfProvided<bool>(args, kWithProxyArg);
    if (with_proxy_result.has_value())
    {
        test_parameters.with_proxy = with_proxy_result.value();
    }

    const auto service_instance_manifest_result =
        score::mw::com::test::GetValueIfProvided<std::string>(args, kServiceInstanceManifestArg);
    if (!service_instance_manifest_result.has_value())
    {
        std::cout << "Test main: Missing or invalid '--" << kServiceInstanceManifestArg
                  << "' argument: " << service_instance_manifest_result.error().Message() << std::endl;
        return {};
    }
    if (service_instance_manifest_result.value().empty())
    {
        std::cout << "Test main: '--" << kServiceInstanceManifestArg << "' must not be empty." << std::endl;
        return {};
    }
    test_parameters.service_instance_manifest = service_instance_manifest_result.value();

    return test_parameters;
}

}  // namespace

int main(int argc, const char** argv)
{
    // Prerequisites for the test steps/sequence
    score::cpp::stop_source test_stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(test_stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Test main: Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing.\n";
    }

    const auto test_parameters_result = ReadCommandLineArguments(argc, argv);
    if (!(test_parameters_result.has_value()))
    {
        std::cerr << "Test main: Could not parse test parameters, exiting." << std::endl;
        return EXIT_FAILURE;
    }
    const auto& test_parameters = test_parameters_result.value();
    score::cpp::set_assertion_handler(&score::mw::com::test::assertion_stdout_handler);

    for (std::size_t test_iteration = 1U; test_iteration <= test_parameters.number_test_iterations; ++test_iteration)
    {
        std::cerr << "Test Main: Running iteration " << test_iteration << " of "
                  << test_parameters.number_test_iterations << " of Provider-Restart-Methods-Test" << std::endl;

        if (!test_parameters.kill_provider)
        {
            if (test_parameters.with_proxy)
            {
                score::mw::com::test::DoProviderNormalRestartWithProxy(test_stop_source.get_token(),
                                                                       test_parameters.service_instance_manifest);
            }
            else
            {
                score::mw::com::test::DoProviderNormalRestartWithoutProxy(test_stop_source.get_token(),
                                                                          test_parameters.service_instance_manifest);
            }
        }
        else
        {
            if (test_parameters.with_proxy)
            {
                score::mw::com::test::DoProviderKillRestartWithProxy(test_stop_source.get_token(),
                                                                     test_parameters.service_instance_manifest);
            }
            else
            {
                score::mw::com::test::DoProviderKillRestartWithoutProxy(test_stop_source.get_token(),
                                                                        test_parameters.service_instance_manifest);
            }
        }
    }

    return EXIT_SUCCESS;
}
