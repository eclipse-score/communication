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
class LolaGetNewSamplesBenchmarkFixture : public benchmark::Fixture
{
  public:
    // Bring base class SetUp/TearDown into scope to avoid hiding them
    using benchmark::Fixture::SetUp;
    using benchmark::Fixture::TearDown;

    LolaGetNewSamplesBenchmarkFixture()
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
        // This flag prevent to call mw::com::runtime to attempt to inizialize every time we use fixture in the same
        // benchmark process.
        if (!fixture_initialized_)
        {
            // clang-format off
            auto config_path = runtime::RuntimeConfiguration(
        "score/mw/com/performance_benchmarks/api_microbenchmarks/config/mw_com_config_qm_high_frequency_send_large_data.json");
            // clang-format on
            score::mw::com::runtime::InitializeRuntime(config_path);
            fixture_initialized_ = true;
        }

        // Created Skeleton
        auto skeleton_result_ =
            TestDataSkeleton::Create(InstanceSpecifier::Create(std::string{kBenchmarkInstanceSpecifier}).value());
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(skeleton_result_.has_value());
        skeleton_ = std::move(skeleton_result_.value());

        // Offer the service
        auto offer_result = skeleton_->OfferService();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(offer_result.has_value());

        // Find and create proxy
        auto handle =
            TestDataProxy::FindService(InstanceSpecifier::Create(std::string{kBenchmarkInstanceSpecifier}).value());
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(handle.has_value());
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(!handle.value().empty());

        auto proxy_result_ = TestDataProxy::Create(handle.value().front());
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_result_.has_value());
        proxy_ = std::move(proxy_result_.value());

        auto subscribe_result = proxy_->test_event.Subscribe(kConfig.max_num_samples);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(subscribe_result.has_value());
    }

    void MakeSenderThread(const score::cpp::stop_token stop_token)
    {
        sender_thread_ = std::thread([this, stop_token]() {
            while (!stop_token.stop_requested())
            {
                auto sample_alloc_result = skeleton_->test_event.Allocate();
                if (!sample_alloc_result.has_value())
                {
                    break;
                }
                auto sample = std::move(sample_alloc_result).value();
                std::fill(sample->begin(), sample->end(), kConfig.fill_data);
                skeleton_->test_event.Send(std::move(sample));
                std::this_thread::sleep_for(std::chrono::milliseconds{kConfig.send_cycle_time_ms});
            }
        });
    }

    void TearDown(const benchmark::State& /*state*/) override
    {
        // Join sender thread
        if (sender_thread_.joinable())
        {
            sender_thread_.join();
        }
        // Unsubscribe from event and destroy proxy before destroying skeleton
        if (proxy_.has_value())
        {
            proxy_->test_event.Unsubscribe();
            proxy_.reset();
        }
        // Stop offering service and destroy skeleton
        if (skeleton_.has_value())
        {
            skeleton_->StopOfferService();
            skeleton_.reset();
        }
    }

  protected:
    std::optional<TestDataSkeleton> skeleton_;
    std::optional<TestDataProxy> proxy_;
    std::thread sender_thread_;
    static std::atomic<bool> fixture_initialized_;
};

std::atomic<bool> LolaGetNewSamplesBenchmarkFixture::fixture_initialized_{false};

BENCHMARK_F(LolaGetNewSamplesBenchmarkFixture, GetNewSamples)(benchmark::State& state)
{

    std::cout << "GetNewSamples Run: " << gGetNewSamplesBenchmarkIndex++ << '\n';
    score::cpp::stop_source stopper{};
    MakeSenderThread(stopper.get_token());

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(proxy_->test_event.GetNewSamples(
            [](SamplePtr<DataType> /*sample*/) noexcept {
                // we receive the sample and do nothing with it.
                // We could come up with a check to validate at least the size of the sample here.
            },
            kConfig.max_num_samples));
    }

    if (!stopper.stop_requested())
    {
        stopper.request_stop();
    }
}

}  // namespace score::mw::com::test

BENCHMARK_MAIN();
