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
#include "score/mw/com/performance_benchmarks/api_microbenchmarks/lola_interface.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/com/types.h"

#include <benchmark/benchmark.h>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <thread>

namespace score::mw::com::test
{

namespace
{

std::size_t gGetNewSamplesBenchmarkIndex{0};
constexpr std::string_view kBenchmarkInstanceSpecifier = "test/lolabenchmark";

struct DataExchangeConfig
{
    std::size_t fill_data{12U};
    std::size_t send_cycle_time_ms{1};
    // NOTE: This variable has a significant impact on the overall runtime of the benchmark
    std::size_t max_num_samples{25};
};

constexpr DataExchangeConfig kConfig{};
}  // namespace

// This fixture will be used to benchmark the LoLa runtime
class LolaAllocateSendBenchmarkFixture : public benchmark::Fixture
{
  public:
    // Bring base class SetUp/TearDown into scope to avoid hiding them
    using benchmark::Fixture::SetUp;
    using benchmark::Fixture::TearDown;

    LolaAllocateSendBenchmarkFixture()
    {
        // This code is run once per benchmark
        this->Repetitions(10);
        this->ReportAggregatesOnly(true);
        this->ThreadRange(1, 1);
        this->MeasureProcessCPUTime();
        this->UseRealTime();
        this->Unit(benchmark::kMicrosecond);
    }

    void SetUp(const benchmark::State& /*state*/) override
    {
        // This code is run once per state update (i.e. once per loop)
        // This flag prevent to call mw::com::runtime to attempt to initialize every time we use fixture in the same
        // benchmark process.
        if (!fixture_initialized_)
        {
            // clang-format off
            const auto config_path = runtime::RuntimeConfiguration(
        "score/mw/com/performance_benchmarks/api_microbenchmarks/config/mw_com_config_qm_high_frequency_send_large_data.json");
            // clang-format on
            score::mw::com::runtime::InitializeRuntime(config_path);
            fixture_initialized_ = true;
        }

        // Create Skeleton
        auto skeleton_result =
            TestDataSkeleton::Create(InstanceSpecifier::Create(std::string{kBenchmarkInstanceSpecifier}).value());
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(skeleton_result.has_value());
        skeleton_ = std::move(skeleton_result).value();

        // Offer the service
        const auto offer_result = skeleton_->OfferService();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(offer_result.has_value());
    }

    void TearDown(const benchmark::State& /*state*/) override
    {
        // Stop offering service and destroy skeleton
        if (skeleton_.has_value())
        {
            skeleton_->StopOfferService();
            skeleton_.reset();
        }
    }

  protected:
    std::optional<TestDataSkeleton> skeleton_;
    static std::atomic<bool> fixture_initialized_;
};

std::atomic<bool> LolaAllocateSendBenchmarkFixture::fixture_initialized_{false};

BENCHMARK_F(LolaAllocateSendBenchmarkFixture, AllocateSend)(benchmark::State& state)
{

    std::cout << "AllocateSend Run: " << gGetNewSamplesBenchmarkIndex++ << '\n';

    auto allocate_send_sequence = [&state](TestDataSkeleton& skeleton) {
        auto sample_alloc_result = skeleton.test_event.Allocate();
        if (!sample_alloc_result.has_value())
        {
            state.SkipWithError("Allocate Failed");
            return;
        }
        auto sample = std::move(sample_alloc_result).value();
        const auto send_result = skeleton.test_event.Send(std::move(sample));
        if (!send_result.has_value())
        {
            state.SkipWithError("Send Failed");
        }
    };

    for (auto _ : state)
    {
        std::ignore = _;
        allocate_send_sequence(*skeleton_);
    }
}

}  // namespace score::mw::com::test

BENCHMARK_MAIN();
