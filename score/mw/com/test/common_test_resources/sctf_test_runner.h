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


#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SCTF_TEST_RUNNER_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SCTF_TEST_RUNNER_H

#include <amp_optional.hpp>
#include <amp_stop_token.hpp>

#include <algorithm>
#include <chrono>
#include <future>
#include <string>
#include <vector>

namespace bmw
{
namespace mw
{
namespace com
{
namespace test
{

class SctfTestRunner
{
  public:
    class RunParameters
    {
      public:
        enum class Parameters
        {
            CYCLE_TIME,
            MODE,
            NUM_CYCLES,
            SERVICE_INSTANCE_MANIFEST,
            UID,
            NUM_RETRIES,
            RETRY_BACKOFF_TIME,
            SHOULD_MODIFY_DATA_SEGMENT,
        };

        RunParameters(const std::vector<RunParameters::Parameters>& allowed_parameters,
                      amp::optional<std::chrono::milliseconds> cycle_time,
                      amp::optional<std::string> mode,
                      amp::optional<std::size_t> num_cycles,
                      amp::optional<std::string> service_instance_manifest,
                      amp::optional<uid_t> uid,
                      amp::optional<std::size_t> num_retries,
                      amp::optional<std::chrono::milliseconds> retry_backoff_time,
                      amp::optional<bool> should_modify_data_segment) noexcept;

        std::chrono::milliseconds GetCycleTime() const;
        std::string GetMode() const;
        std::size_t GetNumCycles() const;
        std::string GetServiceInstanceManifest() const;
        uid_t GetUid() const;
        std::size_t GetNumRetries() const;
        std::chrono::milliseconds GetRetryBackoffTime() const;
        bool GetShouldModifyDataSegment() const;

        amp::optional<std::chrono::milliseconds> GetOptionalCycleTime() const;
        amp::optional<std::string> GetOptionalMode() const;
        amp::optional<std::size_t> GetOptionalNumCycles() const;
        amp::optional<std::string> GetOptionalServiceInstanceManifest() const;
        amp::optional<uid_t> GetOptionalUid() const;
        amp::optional<std::size_t> GetOptionalNumRetries() const;
        amp::optional<std::chrono::milliseconds> GetOptionalRetryBackoffTime() const;

      private:
        const std::vector<RunParameters::Parameters> allowed_parameters_;
        const amp::optional<std::chrono::milliseconds> cycle_time_;
        const amp::optional<std::string> mode_;
        const amp::optional<std::size_t> num_cycles_;
        const amp::optional<std::string> service_instance_manifest_;
        const amp::optional<uid_t> uid_;
        const amp::optional<std::size_t> num_retries_;
        const amp::optional<std::chrono::milliseconds> retry_backoff_time_;
        const amp::optional<bool> should_modify_data_segment_;
    };

    SctfTestRunner(int argc, const char** argv, const std::vector<RunParameters::Parameters>& allowed_parameters);

    amp::stop_token GetStopToken() const noexcept { return stop_source_.get_token(); }
    amp::stop_source GetStopSource() noexcept { return stop_source_; }
    RunParameters GetRunParameters() const noexcept { return run_parameters_; }

    static int WaitForAsyncTestResults(std::vector<std::future<int>>& future_return_values);

  private:
    RunParameters ParseCommandLineArguments(int argc,
                                            const char** argv,
                                            const std::vector<RunParameters::Parameters>& allowed_parameters) const;
    void SetupSigTermHandler();

    const RunParameters run_parameters_;
    amp::stop_source stop_source_;
};

}  // namespace test
}  // namespace com
}  // namespace mw
}  // namespace bmw

#endif  // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SCTF_TEST_RUNNER_H
