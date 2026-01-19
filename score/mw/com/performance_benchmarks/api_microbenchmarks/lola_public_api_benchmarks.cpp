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
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/com/types.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <filesystem>
#include <string_view>

namespace score::mw::com::test
{

namespace
{
constexpr std::string_view kBenchmarkInstanceSpecifier = "/score/mw/com/test/TestInterface";
}

// This fixture will be used to benchmark the LoLa runtime
class LolaBenchmarkFixture : public benchmark::Fixture
{
  public:
    LolaBenchmarkFixture()
    {
        this->Repetitions(10);
        this->ReportAggregatesOnly(true);
        this->ThreadRange(1, 1);
        this->UseRealTime();
        this->MeasureProcessCPUTime();

        // This flag prevent to call mw::com::runtime to attempt to inizialize every time we use fixture in the same
        // benchmark process.
        if (!fixture_initialized_)
        {
            auto config_path = runtime::RuntimeConfiguration(
                "score/mw/com/performance_benchmarks/api_microbenchmarks/config/mw_com_config.json");
            score::mw::com::runtime::InitializeRuntime(config_path);
            fixture_initialized_ = true;
        }
    }

    static std::atomic<bool> fixture_initialized_;
};

std::atomic<bool> LolaBenchmarkFixture::fixture_initialized_{false};

BENCHMARK_F(LolaBenchmarkFixture, LoLaInstanceSpecifierCreate)(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = score::mw::com::InstanceSpecifier::Create(std::string{kBenchmarkInstanceSpecifier});
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK_F(LolaBenchmarkFixture, LoLaInstanceSpecifierCreatePartialLoopBenchmark)(benchmark::State& state)
{
    // this benchmark is doing the same thing as LoLaInstanceSpecifierCreate, just with a different approach where we
    // time only part of the loop core and not the whole loop. This is just here to demonstrate and document this
    // teqnique.
    for (auto _ : state)
    {
        // other work could be done here which would not be part of the benchmarked time

        auto start = std::chrono::high_resolution_clock::now();
        auto result = score::mw::com::InstanceSpecifier::Create(std::string{kBenchmarkInstanceSpecifier});
        auto end = std::chrono::high_resolution_clock::now();

        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
}

}  // namespace score::mw::com::test

// Run the benchmark (must be at global scope)
BENCHMARK_MAIN();
