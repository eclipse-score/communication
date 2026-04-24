/*******************************************************************************
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
 *******************************************************************************/

#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include <boost/program_options.hpp>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace score::mw::com::test
{

namespace
{

using Parameters = SctfTestRunner::RunParameters::Parameters;

template <typename ParsedType, typename SavedType = ParsedType>
auto ParseAndPackage(const std::unordered_map<std::string, std::string>& args, const std::string& name)
    -> score::cpp::optional<SavedType>
{
    auto result = GetValueIfProvided<ParsedType>(args, name);
    if (!result.has_value())
    {
        std::cerr << "Optional parameter " << name << " not provided... continuing\n";
        return score::cpp::optional<SavedType>{};
    }
    return static_cast<SavedType>(result.value());
}

auto ParameterToNameAndDescription(const Parameters parameter) -> std::pair<std::string, std::string>
{
    switch (parameter)
    {
        case Parameters::CYCLE_TIME:
            return {"cycle-time", "Cycle time in milliseconds for sending/polling"};
        case Parameters::MODE:
            return {"mode", "Set to either send/skeleton or recv/proxy to determine the role of the process"};
        case Parameters::NUM_CYCLES:
            return {"num-cycles",
                    "Number of cycles that are executed before determining success or failure. 0 indicates no limit."};
        case Parameters::SERVICE_INSTANCE_MANIFEST:
            return {"service_instance_manifest", "Path to the com configuration file"};
        case Parameters::UID:
            return {"uid", "UID to setuid to before the actual test run."};
        case Parameters::NUM_RETRIES:
            return {"num-retries", "Number of retries done before determining success or failure."};
        case Parameters::RETRY_BACKOFF_TIME:
            return {"backoff-time", "Waiting time in milliseconds before a retry is attempted."};
        case Parameters::SHOULD_MODIFY_DATA_SEGMENT:
            return {"should-modify-data-segment", "Whether the test should try to modify the data segment."};
        default:
            throw std::runtime_error("Invalid parameter, terminating.");
    }
}

template <typename T>
void AssertParameterExists(const score::cpp::optional<T>& value, const Parameters parameter)
{
    if (!value.has_value())
    {
        throw std::runtime_error(ParameterToNameAndDescription(parameter).first +
                                 " not specified as run parameter, terminating.");
    }
}

void AssertParameterInAllowList(const Parameters parameter, const std::vector<Parameters>& allowed_parameters)
{
    if (std::find(allowed_parameters.begin(), allowed_parameters.end(), parameter) == allowed_parameters.cend())
    {
        throw std::runtime_error(ParameterToNameAndDescription(parameter).first +
                                 " not specified in allowed parameter list, terminating.");
    }
}

}  // namespace

SctfTestRunner::RunParameters::RunParameters(const std::vector<Parameters>& allowed_parameters,
                                             score::cpp::optional<std::chrono::milliseconds> cycle_time,
                                             score::cpp::optional<std::string> mode,
                                             score::cpp::optional<std::size_t> num_cycles,
                                             score::cpp::optional<std::string> service_instance_manifest,
                                             score::cpp::optional<uid_t> uid,
                                             score::cpp::optional<std::size_t> num_retries,
                                             score::cpp::optional<std::chrono::milliseconds> retry_backoff_time,
                                             score::cpp::optional<bool> should_modify_data_segment) noexcept
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

score::cpp::optional<std::chrono::milliseconds> SctfTestRunner::RunParameters::GetOptionalCycleTime() const
{
    AssertParameterInAllowList(Parameters::CYCLE_TIME, allowed_parameters_);
    return cycle_time_;
}

score::cpp::optional<std::string> SctfTestRunner::RunParameters::GetOptionalMode() const
{
    AssertParameterInAllowList(Parameters::MODE, allowed_parameters_);
    return mode_;
}

score::cpp::optional<std::size_t> SctfTestRunner::RunParameters::GetOptionalNumCycles() const
{
    AssertParameterInAllowList(Parameters::NUM_CYCLES, allowed_parameters_);
    return num_cycles_;
}

score::cpp::optional<std::string> SctfTestRunner::RunParameters::GetOptionalServiceInstanceManifest() const
{
    AssertParameterInAllowList(Parameters::SERVICE_INSTANCE_MANIFEST, allowed_parameters_);
    return service_instance_manifest_;
}

score::cpp::optional<uid_t> SctfTestRunner::RunParameters::GetOptionalUid() const
{
    AssertParameterInAllowList(Parameters::UID, allowed_parameters_);
    return uid_;
}

score::cpp::optional<std::size_t> SctfTestRunner::RunParameters::GetOptionalNumRetries() const
{
    AssertParameterInAllowList(Parameters::NUM_RETRIES, allowed_parameters_);
    return num_retries_;
}

score::cpp::optional<std::chrono::milliseconds> SctfTestRunner::RunParameters::GetOptionalRetryBackoffTime() const
{
    AssertParameterInAllowList(Parameters::RETRY_BACKOFF_TIME, allowed_parameters_);
    return retry_backoff_time_;
}

SctfTestRunner::SctfTestRunner(int argc, const char** argv, const std::vector<Parameters>& allowed_parameters)
    : run_parameters_{ParseCommandLineArguments(argc, argv, allowed_parameters)}, stop_source_{}
{
    SetupSigTermHandler();

    const auto& uid = run_parameters_.GetOptionalUid();
    if (uid.has_value())
    {
        if (::setuid(uid.value()) != 0)
        {
            std::cerr << "setuid failed: " << strerror(errno) << std::endl;
        }
    }
    score::mw::com::runtime::InitializeRuntime(argc, argv);
}

void SctfTestRunner::SetupSigTermHandler()
{
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source_);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }
}

SctfTestRunner::RunParameters SctfTestRunner::ParseCommandLineArguments(
    int argc,
    const char** argv,
    const std::vector<Parameters>& allowed_parameters) const
{
    // UID is needed internally by SctfTestRunner
    std::vector<Parameters> allowed_parameters_with_uid(allowed_parameters.begin(), allowed_parameters.end());
    allowed_parameters_with_uid.push_back(Parameters::UID);

    std::vector<std::pair<std::string, std::string>> prameter_descriptor_pairs{{"help", "Display the help message"}};

    for (const auto& parameter : allowed_parameters_with_uid)
    {
        prameter_descriptor_pairs.emplace_back(ParameterToNameAndDescription(parameter));
    }

    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, prameter_descriptor_pairs);

    auto cycle_time = ParseAndPackage<std::size_t, std::chrono::milliseconds>(args, "cycle-time");
    auto mode = ParseAndPackage<std::string>(args, "mode");
    auto num_cycles = ParseAndPackage<std::size_t>(args, "num-cycles");
    auto service_instance_manifest = ParseAndPackage<std::string>(args, "service_instance_manifest");
    auto uid = ParseAndPackage<uid_t>(args, "uid");
    auto num_retry = ParseAndPackage<std::size_t>(args, "num-retries");
    auto retry_backoff_time = ParseAndPackage<std::size_t, std::chrono::milliseconds>(args, "backoff-time");
    auto should_modify_data_segment = ParseAndPackage<bool>(args, "should-modify-data-segment");

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
}  // namespace

int SctfTestRunner::WaitForAsyncTestResults(std::vector<std::future<int>>& future_return_values)
{
    // Wait for all threads to finish and check that they finished safely
    for (auto& future_return_value : future_return_values)
    {
        const auto return_value = future_return_value.get();
        if (return_value != EXIT_SUCCESS)
        {
            return return_value;
        }
    }
    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
