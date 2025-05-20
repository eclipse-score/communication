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

#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include <boost/program_options.hpp>
#include <stdexcept>
#include <utility>

namespace bmw
{
    namespace mw
    {
        namespace com
        {
            namespace test
            {

                namespace
                {

                    using Parameters = SctfTestRunner::RunParameters::Parameters;

                    template <typename ParsedType, typename SavedType = ParsedType>
                    amp::optional<SavedType> GetValueIfProvided(const boost::program_options::variables_map &args, std::string arg_string)
                    {
                        return (args.count(arg_string) > 0U) ? static_cast<SavedType>(args[arg_string].as<ParsedType>())
                                                             : amp::optional<SavedType>();
                    }

                    std::string ParameterToString(const Parameters parameter)
                    {
                        switch (parameter)
                        {
                        case Parameters::CYCLE_TIME:
                            return "cycle_time";
                        case Parameters::MODE:
                            return "mode";
                        case Parameters::NUM_CYCLES:
                            return "num_cycles";
                        case Parameters::SERVICE_INSTANCE_MANIFEST:
                            return "service_instance_manifest";
                        case Parameters::UID:
                            return "uid";
                        case Parameters::NUM_RETRIES:
                            return "num_retries";
                        case Parameters::RETRY_BACKOFF_TIME:
                            return "retry_backoff_time";
                        case Parameters::SHOULD_MODIFY_DATA_SEGMENT:
                            return "should_modify_data_segment";
                        default:
                            throw std::runtime_error("Invalid parameter, terminating.");
                        }
                    }

                    template <typename T>
                    void AssertParameterExists(const amp::optional<T> value, const Parameters parameter)
                    {
                        if (!value.has_value())
                        {
                            throw std::runtime_error(ParameterToString(parameter) + " not specified as run parameter, terminating.");
                        }
                    }

                    void AssertParameterInAllowList(const Parameters parameter, const std::vector<Parameters> &allowed_parameters)
                    {
                        if (std::find(allowed_parameters.begin(), allowed_parameters.end(), parameter) == allowed_parameters.cend())
                        {
                            throw std::runtime_error(ParameterToString(parameter) +
                                                     " not specified in allowed parameter list, terminating.");
                        }
                    }

                } // namespace

                SctfTestRunner::RunParameters::RunParameters(const std::vector<Parameters> &allowed_parameters,
                                                             amp::optional<std::chrono::milliseconds> cycle_time,
                                                             amp::optional<std::string> mode,
                                                             amp::optional<std::size_t> num_cycles,
                                                             amp::optional<std::string> service_instance_manifest,
                                                             amp::optional<uid_t> uid,
                                                             amp::optional<std::size_t> num_retries,
                                                             amp::optional<std::chrono::milliseconds> retry_backoff_time,
                                                             amp::optional<bool> should_modify_data_segment) noexcept
                    : allowed_parameters_{allowed_parameters},
                      cycle_time_{std::move(cycle_time)},
                      mode_{std::move(mode)},
                      num_cycles_{std::move(num_cycles)},
                      service_instance_manifest_{std::move(service_instance_manifest)},
                      uid_{std::move(uid)},
                      num_retries_{std::move(num_retries)},
                      retry_backoff_time_{std::move(retry_backoff_time)},
                      should_modify_data_segment_{std::move(should_modify_data_segment)}
                {
                }

                std::chrono::milliseconds SctfTestRunner::RunParameters::GetCycleTime() const
                {
                    AssertParameterInAllowList(Parameters::CYCLE_TIME, allowed_parameters_);
                    AssertParameterExists(cycle_time_, Parameters::CYCLE_TIME);
                    return cycle_time_.value();
                }

                std::string SctfTestRunner::RunParameters::GetMode() const
                {
                    AssertParameterInAllowList(Parameters::MODE, allowed_parameters_);
                    AssertParameterExists(mode_, Parameters::MODE);
                    return mode_.value();
                }

                std::size_t SctfTestRunner::RunParameters::GetNumCycles() const
                {
                    AssertParameterInAllowList(Parameters::NUM_CYCLES, allowed_parameters_);
                    AssertParameterExists(num_cycles_, Parameters::NUM_CYCLES);
                    return num_cycles_.value();
                }

                std::string SctfTestRunner::RunParameters::GetServiceInstanceManifest() const
                {
                    AssertParameterInAllowList(Parameters::SERVICE_INSTANCE_MANIFEST, allowed_parameters_);
                    AssertParameterExists(service_instance_manifest_, Parameters::SERVICE_INSTANCE_MANIFEST);
                    return service_instance_manifest_.value();
                }

                uid_t SctfTestRunner::RunParameters::GetUid() const
                {
                    AssertParameterInAllowList(Parameters::UID, allowed_parameters_);
                    AssertParameterExists(uid_, Parameters::UID);
                    return uid_.value();
                }

                std::size_t SctfTestRunner::RunParameters::GetNumRetries() const
                {
                    AssertParameterInAllowList(Parameters::NUM_RETRIES, allowed_parameters_);
                    AssertParameterExists(num_retries_, Parameters::NUM_RETRIES);
                    return num_retries_.value();
                }

                std::chrono::milliseconds SctfTestRunner::RunParameters::GetRetryBackoffTime() const
                {
                    AssertParameterInAllowList(Parameters::RETRY_BACKOFF_TIME, allowed_parameters_);
                    AssertParameterExists(retry_backoff_time_, Parameters::RETRY_BACKOFF_TIME);
                    return retry_backoff_time_.value();
                }

                bool SctfTestRunner::RunParameters::GetShouldModifyDataSegment() const
                {
                    AssertParameterInAllowList(Parameters::SHOULD_MODIFY_DATA_SEGMENT, allowed_parameters_);
                    AssertParameterExists(should_modify_data_segment_, Parameters::SHOULD_MODIFY_DATA_SEGMENT);
                    return should_modify_data_segment_.value();
                }

                amp::optional<std::chrono::milliseconds> SctfTestRunner::RunParameters::GetOptionalCycleTime() const
                {
                    AssertParameterInAllowList(Parameters::CYCLE_TIME, allowed_parameters_);
                    return cycle_time_;
                }

                amp::optional<std::string> SctfTestRunner::RunParameters::GetOptionalMode() const
                {
                    AssertParameterInAllowList(Parameters::MODE, allowed_parameters_);
                    return mode_;
                }

                amp::optional<std::size_t> SctfTestRunner::RunParameters::GetOptionalNumCycles() const
                {
                    AssertParameterInAllowList(Parameters::NUM_CYCLES, allowed_parameters_);
                    return num_cycles_;
                }

                amp::optional<std::string> SctfTestRunner::RunParameters::GetOptionalServiceInstanceManifest() const
                {
                    AssertParameterInAllowList(Parameters::SERVICE_INSTANCE_MANIFEST, allowed_parameters_);
                    return service_instance_manifest_;
                }

                amp::optional<uid_t> SctfTestRunner::RunParameters::GetOptionalUid() const
                {
                    AssertParameterInAllowList(Parameters::UID, allowed_parameters_);
                    return uid_;
                }

                amp::optional<std::size_t> SctfTestRunner::RunParameters::GetOptionalNumRetries() const
                {
                    AssertParameterInAllowList(Parameters::NUM_RETRIES, allowed_parameters_);
                    return num_retries_;
                }

                amp::optional<std::chrono::milliseconds> SctfTestRunner::RunParameters::GetOptionalRetryBackoffTime() const
                {
                    AssertParameterInAllowList(Parameters::RETRY_BACKOFF_TIME, allowed_parameters_);
                    return retry_backoff_time_;
                }

                SctfTestRunner::SctfTestRunner(int argc, const char **argv, const std::vector<Parameters> &allowed_parameters)
                    : run_parameters_{ParseCommandLineArguments(argc, argv, allowed_parameters)}, stop_source_{}
                {
                    SetupSigTermHandler();

                    const auto &uid = run_parameters_.GetOptionalUid();
                    if (uid.has_value())
                    {
                        if (::setuid(uid.value()) != 0)
                        {
                            std::cerr << "setuid failed: " << strerror(errno) << std::endl;
                        }
                    }
                    bmw::mw::com::runtime::InitializeRuntime(argc, argv);
                }

                void SctfTestRunner::SetupSigTermHandler()
                {
                    const bool sig_term_handler_setup_success = bmw::mw::com::SetupStopTokenSigTermHandler(stop_source_);
                    if (!sig_term_handler_setup_success)
                    {
                        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
                    }
                }

                SctfTestRunner::RunParameters SctfTestRunner::ParseCommandLineArguments(
                    int argc,
                    const char **argv,
                    const std::vector<Parameters> &allowed_parameters) const
                {
                    // UID is needed internally by SctfTestRunner
                    std::vector<Parameters> allowed_parameters_with_uid(allowed_parameters.begin(), allowed_parameters.end());
                    allowed_parameters_with_uid.push_back(Parameters::UID);

                    namespace po = boost::program_options;

                    po::options_description options;
                    // clang-format off
    options.add_options()
        ("help,h", "Display the help message");

    for (const auto& parameter : allowed_parameters_with_uid)
    {
        if (parameter == Parameters::CYCLE_TIME)
        {
            options.add_options()
                ("cycle-time,t", po::value<std::size_t>(), "Cycle time in milliseconds for sending/polling");
        }
        else if (parameter == Parameters::MODE)
        {
            options.add_options()
                ("mode,m", po::value<std::string>(), "Set to either send/skeleton or recv/proxy to determine the "
                                                     "role of the process");
        }
        else if (parameter == Parameters::NUM_CYCLES)
        {
            options.add_options()
                ("num-cycles,n", po::value<std::size_t>()->default_value(0U),
                "Number of cycles that are executed before determining success or failure. 0 indicates no limit.");
        }
        else if (parameter == Parameters::SERVICE_INSTANCE_MANIFEST)
        {
            options.add_options()
                ("service_instance_manifest,s", po::value<std::string>(), "Path to the com configuration file");
        }
        else if (parameter == Parameters::UID)
        {
            options.add_options()
                ("uid,u", po::value<uid_t>(), "UID to setuid to before the actual test run.");
        }
        else if (parameter == Parameters::NUM_RETRIES)
        {
            options.add_options()
                ("num-retries,r", po::value<std::size_t>(), "Number of retries done before determining success or failure.");
        }
        else if (parameter == Parameters::RETRY_BACKOFF_TIME)
        {
            options.add_options()
                ("backoff-time,b", po::value<std::size_t>(), "Waiting time in milliseconds before a retry is attempted.");
        }
        else if (parameter == Parameters::SHOULD_MODIFY_DATA_SEGMENT)
        {
            options.add_options()
                ("should-modify-data-segment", po::value<bool>(), "Whether the test should try to modify the data segment.");
        }
    }
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
                        throw std::runtime_error("Could not parse command line arguments");
                    }

                    po::notify(args);

                    auto cycle_time = GetValueIfProvided<std::size_t, std::chrono::milliseconds>(args, "cycle-time");
                    auto mode = GetValueIfProvided<std::string>(args, "mode");
                    auto num_cycles = GetValueIfProvided<std::size_t>(args, "num-cycles");
                    auto service_instance_manifest = GetValueIfProvided<std::string>(args, "service_instance_manifest");
                    auto uid = GetValueIfProvided<uid_t>(args, "uid");
                    auto num_retry = GetValueIfProvided<std::size_t>(args, "num-retries");
                    auto retry_backoff_time = GetValueIfProvided<std::size_t, std::chrono::milliseconds>(args, "backoff-time");
                    auto should_modify_data_segment = GetValueIfProvided<bool>(args, "should-modify-data-segment");

                    RunParameters run_parameters(allowed_parameters_with_uid,
                                                 std::move(cycle_time),
                                                 std::move(mode),
                                                 std::move(num_cycles),
                                                 std::move(service_instance_manifest),
                                                 std::move(uid),
                                                 std::move(num_retry),
                                                 std::move(retry_backoff_time),
                                                 std::move(should_modify_data_segment));

                    return run_parameters;
                }

                int SctfTestRunner::WaitForAsyncTestResults(std::vector<std::future<int>> &future_return_values)
                {
                    // Wait for all threads to finish and check that they finished safely
                    for (auto &future_return_value : future_return_values)
                    {
                        const auto return_value = future_return_value.get();
                        if (return_value != EXIT_SUCCESS)
                        {
                            return return_value;
                        }
                    }
                    return EXIT_SUCCESS;
                }

            } // namespace test
        } // namespace com
    } // namespace mw
} // namespace bmw
