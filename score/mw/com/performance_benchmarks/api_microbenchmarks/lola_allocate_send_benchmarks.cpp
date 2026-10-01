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

// Common SetUp/TearDown logic shared by both the combined AllocateSend benchmark (automatic
// CPU/wall-clock timing) and the split Allocate()-only/Send()-only benchmarks (manual timing).
namespace
{
class LolaAllocateSendBenchmarkFixtureBase : public benchmark::Fixture
{
  public:
    // Bring base class SetUp/TearDown into scope to avoid hiding them
    using benchmark::Fixture::SetUp;
    using benchmark::Fixture::TearDown;

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
}  // namespace

std::atomic<bool> LolaAllocateSendBenchmarkFixtureBase::fixture_initialized_{false};

// Fixture for the combined AllocateSend benchmark: uses Google Benchmark's automatic (CPU/wall-clock)
// timing, exactly as before.
class LolaAllocateSendBenchmarkFixture : public LolaAllocateSendBenchmarkFixtureBase
{
  public:
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
};

// Fixture for the AllocateOnly/SendOnly benchmarks below.
// These use Google Benchmark's *manual* timing mode (UseManualTime() + state.SetIterationTime()).
// Rationale: to isolate the cost of Allocate() alone or Send() alone out of the combined ~230ns
// AllocateSend measurement, every iteration needs some untimed "setup"/"drain" work:
//   - AllocateOnly: the SampleAllocateePtr returned by Allocate() must be released before the next
//     Allocate() call can succeed (slots are a finite resource). We do this by simply letting it go
//     out of scope - but its destructor cost must NOT be attributed to Allocate()'s measured time.
//   - SendOnly: Send() always needs a freshly allocated SampleAllocateePtr to consume. We call
//     Allocate() first - but its cost must NOT be attributed to Send()'s measured time.
// Automatic timing (the default for/for-each loop based BENCHMARK_F body) times the *entire* loop
// body each iteration, so it cannot exclude this extra setup/drain work. Pausing/resuming the
// automatic timer (state.PauseTiming()/ResumeTiming()) is documented by Google Benchmark as
// comparatively expensive and not recommended for fine-grained, sub-microsecond measurements like
// these. Manual timing avoids that overhead: we take two raw std::chrono::steady_clock readings
// bracketing only the call of interest, and report just that delta via SetIterationTime() - the
// untimed setup/drain code runs outside those two readings.
class LolaAllocateSendManualTimeBenchmarkFixture : public LolaAllocateSendBenchmarkFixtureBase
{
  public:
    LolaAllocateSendManualTimeBenchmarkFixture()
    {
        // This code is run once per benchmark
        this->Repetitions(10);
        this->ReportAggregatesOnly(true);
        this->ThreadRange(1, 1);
        this->UseManualTime();
        this->Unit(benchmark::kMicrosecond);
    }
};

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

BENCHMARK_F(LolaAllocateSendManualTimeBenchmarkFixture, AllocateOnly)(benchmark::State& state)
{
    std::cout << "AllocateOnly Run: " << gGetNewSamplesBenchmarkIndex++ << '\n';

    for (auto _ : state)
    {
        std::ignore = _;

        const auto t_start = std::chrono::steady_clock::now();
        auto sample_alloc_result = skeleton_->test_event.Allocate();
        const auto t_end = std::chrono::steady_clock::now();

        if (!sample_alloc_result.has_value())
        {
            state.SkipWithError("Allocate Failed");
            break;
        }
        state.SetIterationTime(std::chrono::duration<double>(t_end - t_start).count());

        // sample_alloc_result (and the SampleAllocateePtr it contains) is destroyed here, outside the
        // timed region, releasing the event slot so that the next Allocate() call can succeed. We
        // deliberately do NOT call Send() on it, and we deliberately do NOT include this destruction
        // in the measured time.
    }
}

BENCHMARK_F(LolaAllocateSendManualTimeBenchmarkFixture, SendOnly)(benchmark::State& state)
{
    std::cout << "SendOnly Run: " << gGetNewSamplesBenchmarkIndex++ << '\n';

    for (auto _ : state)
    {
        std::ignore = _;

        // Untimed setup: Send() always needs a freshly allocated SampleAllocateePtr to consume. This
        // Allocate() call's cost must NOT be attributed to Send()'s measured time.
        auto sample_alloc_result = skeleton_->test_event.Allocate();
        if (!sample_alloc_result.has_value())
        {
            state.SkipWithError("Allocate Failed");
            break;
        }
        auto sample = std::move(sample_alloc_result).value();

        const auto t_start = std::chrono::steady_clock::now();
        const auto send_result = skeleton_->test_event.Send(std::move(sample));
        const auto t_end = std::chrono::steady_clock::now();

        if (!send_result.has_value())
        {
            state.SkipWithError("Send Failed");
            break;
        }
        state.SetIterationTime(std::chrono::duration<double>(t_end - t_start).count());
    }
}

}  // namespace score::mw::com::test

BENCHMARK_MAIN();
