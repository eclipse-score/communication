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

namespace score::mw::com::test
{

namespace
{
constexpr std::string_view kBenchmarkInstanceSpecifier = "test/lolabenchmark";
}

// This fixture will be used to benchmark the LoLa runtime
class LolaGetNumNewSamplesAvailableBenchmarkFixture : public benchmark::Fixture
{
  public:
    // Bring base class SetUp/TearDown into scope to avoid hiding them
    using benchmark::Fixture::SetUp;
    using benchmark::Fixture::TearDown;

    LolaGetNumNewSamplesAvailableBenchmarkFixture()
    {
        this->Repetitions(10);
        this->ReportAggregatesOnly(true);
        this->ThreadRange(1, 1);
        this->UseRealTime();
        this->MeasureProcessCPUTime();
    }

    void SetUp(const benchmark::State& /*state*/) override
    {
        // This flag prevent to call mw::com::runtime to attempt to inizialize every time we use fixture in the same
        // benchmark process.
        if (!fixture_initialized_)
        {
            auto config_path = runtime::RuntimeConfiguration(
                "score/mw/com/performance_benchmarks/api_microbenchmarks/config/mw_com_config.json");
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

        // Subscribe to the event with capacity for 32 samples
        auto subscribe_result = proxy_->test_event.Subscribe(/*max_num_samples*/ 32);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(subscribe_result.has_value());

        sender_thread_ = std::thread([this]() {
            for (int i = 0; i < 100; ++i)
            {
                auto sample_alloc_result = skeleton_->test_event.Allocate();
                if (!sample_alloc_result.has_value())
                {
                    break;
                }
                auto sample = std::move(sample_alloc_result.value());
                std::fill(sample->begin(), sample->end(), 1U);
                skeleton_->test_event.Send(std::move(sample));
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
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

        // Allow some time for cleanup to complete
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

  protected:
    std::optional<TestDataSkeleton> skeleton_;
    std::optional<TestDataProxy> proxy_;
    std::thread sender_thread_;
    static std::atomic<bool> fixture_initialized_;
};

std::atomic<bool> LolaGetNumNewSamplesAvailableBenchmarkFixture::fixture_initialized_{false};

BENCHMARK_F(LolaGetNumNewSamplesAvailableBenchmarkFixture, GetNumNewSamplesAvailable)(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(proxy_->test_event.GetNumNewSamplesAvailable());
    }
}

}  // namespace score::mw::com::test

BENCHMARK_MAIN();
