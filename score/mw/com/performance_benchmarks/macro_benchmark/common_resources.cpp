/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/performance_benchmarks/macro_benchmark/common_resources.h"
#include "score/mw/com/performance_benchmarks/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/runtime.h"
#include "score/mw/log/logging.h"

#include <score/stop_token.hpp>

#include <boost/program_options.hpp>

namespace score::mw::com::test
{

bool GetStopTokenAndSetUpSigTermHandler(score::cpp::stop_source& test_stop_source)
{

    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(test_stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Test main: Unable to set signal handler for SIGINT and/or SIGTERM.\n";
        return false;
    }
    return true;
}

std::optional<SharedMemoryObjectCreator<CounterType>> GetSharedFlag()
{

    auto proxy_is_done_flag_res = score::mw::com::test::SharedMemoryObjectCreator<CounterType>::CreateOrOpenObject(
        std::string{kProxyFinishedFlagShmPath}, 0U);

    if (!proxy_is_done_flag_res.has_value())
    {
        mw::log::LogError()
            << "Creating interprocess notification object on skeleton side failed: "
            << "This is a problem since the service will never get the notification that this client is done!!! "
            << proxy_is_done_flag_res.error().ToString();
        return std::nullopt;
    }

    return std::move(proxy_is_done_flag_res.value());
}

void test_failure(std::string_view msg, std::string_view log_context)
{
    if (!msg.empty())
    {
        mw::log::LogError(log_context) << msg;
    }

    mw::log::LogError(log_context) << "TEST FAILED!!!";
    std::exit(EXIT_FAILURE);
}

void test_success(std::string_view msg, std::string_view log_context)
{
    if (!msg.empty())
    {
        mw::log::LogInfo(log_context) << msg;
    }

    mw::log::LogInfo(log_context) << "TEST SUCCEEDED!!!";
    std::exit(EXIT_SUCCESS);
}

std::optional<TestCommanLineArguments> ParseCommandLineArgs(int argc, const char** argv, std::string_view log_context)
{
    namespace po = boost::program_options;
    po::options_description options;

    TestCommanLineArguments args;
    // clang-format off
    options.add_options()("help", "Display the help message")
        ("config_path", po::value<std::string>(&args.config_path), "REQUIRED: path to <service|client>_config file. The parser assumes that this config file has been validated against it's schema and conforms to it.")
        ("service_instance_manifest", po::value<std::string>(&args.service_instance_manifest), "optional: path/to/mw_com_config.json. By default ./etc/mw_com_config.json will be chosen.");
    // clang-format on

    po::positional_options_description positional_options;
    positional_options.add("config_path", 1);
    positional_options.add("service_instance_manifest", 1);

    po::variables_map arg_map;
    const auto parsed_args = po::command_line_parser{argc, argv}.positional(positional_options).options(options).run();
    po::store(parsed_args, arg_map);

    if (arg_map.count("help") > 0)
    {
        std::cerr << options << std::endl;
        return std::nullopt;
    }

    if (arg_map.count("config_path") != 1)
    {
        std::cerr << options << std::endl;
        return std::nullopt;
    }

    if (arg_map.count("service_instance_manifest") != 1)
    {
        std::cerr << options << std::endl;
        return std::nullopt;
    }

    po::notify(arg_map);
    score::mw::log::LogInfo(log_context) << "config_path: " << args.config_path;
    score::mw::log::LogInfo(log_context) << "service_instance_manifest: " << args.service_instance_manifest;
    return args;
}

void InitializeRuntime(const std::string& path)
{
    auto rtc = path.empty() ? score::mw::com::runtime::RuntimeConfiguration()
                            : score::mw::com::runtime::RuntimeConfiguration(path);
    score::mw::com::runtime::InitializeRuntime(rtc);
    score::mw::log::LogInfo() << "LoLa Runtime initialized!";
}

}  // namespace score::mw::com::test
