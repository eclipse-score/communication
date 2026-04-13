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
#include "score/mw/com/test/partial_restart/methods/consumer_restart/consumer_restart_application.h"
#include "score/mw/com/test/partial_restart/methods/consumer_restart/controller.h"

#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace
{

/// \brief Test parameters for the ITF test variants for consumer restart with methods.
/// \details We have two variants for consumer restart ITF with methods. This is reflected
///          in the test parameter kill_consumer:
///          ITF variant 1: Consumer graceful/normal restart
///          ITF variant 2: Consumer kill/crash restart
struct TestParameters
{
    std::optional<std::string> service_instance_manifest{};
    std::size_t number_test_iterations{};
    /// \brief shall the consumer be killed (true) or gracefully shutdown (false) before restart.
    bool kill_consumer{false};
};

std::optional<TestParameters> ParseTestParameters(int argc, const char** argv) noexcept
{
    namespace po = boost::program_options;

    TestParameters test_parameters{};

    // Reading in cmd-line params
    po::options_description options;

    std::string service_instance_manifest{};

    // clang-format off
    options.add_options()
        ("help", "Display the help message")
        ("service_instance_manifest", po::value<std::string>(&service_instance_manifest)->default_value(""), "Path to the com configuration file")
        ("turns,t", po::value<std::size_t>(&test_parameters.number_test_iterations), "Number of cycles (consumer restarts) to be done")
        ("kill", po::value<bool>(&test_parameters.kill_consumer), "Shall the consumer get killed before restart or gracefully shutdown?");
    // clang-format on
    po::variables_map args;
    const auto parsed_args =
        po::command_line_parser{argc, argv}
            .options(options)
            .style(po::command_line_style::unix_style | po::command_line_style::allow_long_disguise)
            .run();
    po::store(parsed_args, args);

    if (args.count("help") > 0U)
    {
        std::cerr << options << std::endl;
        return {};
    }

    po::notify(args);

    if (service_instance_manifest != "")
    {
        test_parameters.service_instance_manifest = service_instance_manifest;
    }
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

    const auto test_parameters_result = ParseTestParameters(argc, argv);
    if (!(test_parameters_result.has_value()))
    {
        std::cerr << "Test main: Could not parse test parameters, exiting." << std::endl;
        return EXIT_FAILURE;
    }
    const auto& test_parameters = test_parameters_result.value();
    score::cpp::set_assertion_handler(&score::mw::com::test::assertion_stdout_handler);
    const int mw_com_argc = test_parameters.service_instance_manifest.has_value() ? argc : 0;
    const char** mw_com_argv = test_parameters.service_instance_manifest.has_value() ? argv : nullptr;

    int test_result{};
    for (std::size_t test_iteration = 1U; test_iteration <= test_parameters.number_test_iterations; ++test_iteration)
    {
        std::cerr << "Test Main: Running iteration " << test_iteration << " of "
                  << test_parameters.number_test_iterations << " of Consumer-Restart-Methods-Test" << std::endl;

        if (test_parameters.kill_consumer == false)
        {
            test_result =
                score::mw::com::test::DoConsumerNormalRestart(test_stop_source.get_token(), mw_com_argc, mw_com_argv);
        }
        else
        {
            test_result =
                score::mw::com::test::DoConsumerKillRestart(test_stop_source.get_token(), mw_com_argc, mw_com_argv);
        }

        if (test_result != EXIT_SUCCESS)
        {
            std::cerr << "Test Main: Iteration " << test_iteration << " of " << test_parameters.number_test_iterations
                      << " of Consumer-Restart-Methods-Test FAILED!" << std::endl;
            return EXIT_FAILURE;
        }
    }
    std::cerr << "Test Main: All " << test_parameters.number_test_iterations
              << " iterations of Consumer-Restart-Methods-Test PASSED!" << std::endl;
    return EXIT_SUCCESS;
}
