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

#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SCTF_TEST_RUNNER_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SCTF_TEST_RUNNER_H

#include <score/optional.hpp>
#include <score/stop_token.hpp>

#include <algorithm>
#include <chrono>
#include <future>
#include <string>
#include <vector>

namespace score::mw::com::test
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
                      score::cpp::optional<std::chrono::milliseconds> cycle_time,
                      score::cpp::optional<std::string> mode,
                      score::cpp::optional<std::size_t> num_cycles,
                      score::cpp::optional<std::string> service_instance_manifest,
                      score::cpp::optional<uid_t> uid,
                      score::cpp::optional<std::size_t> num_retries,
                      score::cpp::optional<std::chrono::milliseconds> retry_backoff_time,
                      score::cpp::optional<bool> should_modify_data_segment) noexcept;

        std::chrono::milliseconds GetCycleTime() const;
        std::string GetMode() const;
        std::size_t GetNumCycles() const;
        std::string GetServiceInstanceManifest() const;
        uid_t GetUid() const;
        std::size_t GetNumRetries() const;
        std::chrono::milliseconds GetRetryBackoffTime() const;
        bool GetShouldModifyDataSegment() const;

        score::cpp::optional<std::chrono::milliseconds> GetOptionalCycleTime() const;
        score::cpp::optional<std::string> GetOptionalMode() const;
        score::cpp::optional<std::size_t> GetOptionalNumCycles() const;
        score::cpp::optional<std::string> GetOptionalServiceInstanceManifest() const;
        score::cpp::optional<uid_t> GetOptionalUid() const;
        score::cpp::optional<std::size_t> GetOptionalNumRetries() const;
        score::cpp::optional<std::chrono::milliseconds> GetOptionalRetryBackoffTime() const;

      private:
        const std::vector<RunParameters::Parameters> allowed_parameters_;
        const score::cpp::optional<std::chrono::milliseconds> cycle_time_;
        const score::cpp::optional<std::string> mode_;
        const score::cpp::optional<std::size_t> num_cycles_;
        const score::cpp::optional<std::string> service_instance_manifest_;
        const score::cpp::optional<uid_t> uid_;
        const score::cpp::optional<std::size_t> num_retries_;
        const score::cpp::optional<std::chrono::milliseconds> retry_backoff_time_;
        const score::cpp::optional<bool> should_modify_data_segment_;
    };

    SctfTestRunner(int argc, const char** argv, const std::vector<RunParameters::Parameters>& allowed_parameters);

    score::cpp::stop_token GetStopToken() const noexcept
    {
        return stop_source_.get_token();
    }
    score::cpp::stop_source GetStopSource() noexcept
    {
        return stop_source_;
    }
    RunParameters GetRunParameters() const noexcept
    {
        return run_parameters_;
    }

    static int WaitForAsyncTestResults(std::vector<std::future<int>>& future_return_values);

  private:
    RunParameters ParseCommandLineArguments(int argc,
                                            const char** argv,
                                            const std::vector<RunParameters::Parameters>& allowed_parameters) const;
    void SetupSigTermHandler();

    const RunParameters run_parameters_;
    score::cpp::stop_source stop_source_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SCTF_TEST_RUNNER_H
