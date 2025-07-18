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
#include "score/mw/com/impl/instance_specifier.h"
#include "benchmark.h"

#include "score/concurrency/notification.h"

#include <cstdlib>
#include <iostream>
#include <sys/syscall.h>
#include <thread>
#include <score/latch.hpp>

#include "score/mw/com/runtime.h"

using namespace std::chrono_literals;
using namespace score::mw::com;
using namespace std::chrono;

constexpr uint32_t kIterations = 10000000 / 1000;

std::mutex cout_mutex{};
score::cpp::latch benchmark_start_point{3}, benchmark_finish_point{3}, init_sync_point{3}, deinit_sync_point{2};
const auto instance_specifier_resultA2B = InstanceSpecifier::Create("benchmark/A2B");
const auto instance_specifier_resultB2A = InstanceSpecifier::Create("benchmark/B2A");

score::cpp::optional<std::reference_wrapper<impl::ProxyEvent<DummyBenchmarkData>>> GetBenchmarkDataProxyEvent(
    BenchmarkProxy& proxy)
{
    return proxy.dummy_benchmark_data_;
}

void SetupThread(int cpu)
{
    auto id = std::this_thread::get_id();
    auto native_handle = *reinterpret_cast<std::thread::native_handle_type*>(&id);

    int max_priority = sched_get_priority_max(SCHED_RR);
    struct sched_param params;
    params.sched_priority = max_priority;
    if (pthread_setschedparam(native_handle, SCHED_RR, &params))
    {
        std::cout << "Failed to setschedparam: " << std::strerror(errno) << std::endl;
        std::cout << "App needs to be run as root" << std::endl;
        return;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    if (pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &cpuset))
    {
        std::cout << "Failed to setaffinity_np: " << std::strerror(errno) << std::endl;
        std::cout << "App needs to be run as root" << std::endl;
    }
}

void Transmitter(bool starter, impl::InstanceSpecifier skeleton_instance_specifier, impl::InstanceSpecifier proxy_instance_specifier)
{
    SetupThread(1);

    auto create_result = BenchmarkSkeleton::Create(skeleton_instance_specifier);
    if (!create_result.has_value())
    {
        std::cerr << "Unable to construct skeleton: " << create_result.error() << ", bailing!\n";
        return;
    }
    auto& skeleton = create_result.value();
    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer service for skeleton: " << offer_result.error() << ", bailing!\n";
        return;
    }

    ServiceHandleContainer<impl::HandleType> handle{};
    do
    {
        auto handles_result = BenchmarkProxy::FindService(proxy_instance_specifier);
        if (!handles_result.has_value())
        {
            std::cerr << "Unable to find service: " << handles_result.error() << ", bailing!\n";
            return;
        }
        handle = std::move(handles_result).value();
        if (handle.size() == 0)
        {
            std::this_thread::sleep_for(500ms);
        }
    } while (handle.size() == 0);

    auto proxy_result = BenchmarkProxy::Create(std::move(handle.front()));
    if (!proxy_result.has_value())
    {
        std::cerr << "Unable to construct proxy: " << proxy_result.error() << ", bailing!\n";
        return;
    }
    auto& proxy = proxy_result.value();

    auto dummy_data_event_optional = GetBenchmarkDataProxyEvent(proxy);
    if (!dummy_data_event_optional.has_value())
    {
        std::cerr << "Could not get dummy_data proxy event\n";
        return;
    }
    impl::ProxyEvent<score::mw::com::DummyBenchmarkData>& dummy_data_event = dummy_data_event_optional.value().get();

    dummy_data_event.Subscribe(1);

    init_sync_point.arrive_and_wait();
    benchmark_start_point.arrive_and_wait();

    if (starter)
    {
        skeleton.dummy_benchmark_data_.Send(std::move(skeleton.dummy_benchmark_data_.Allocate()).value());
    }

    for (std::size_t cycle = 0U; cycle < kIterations; cycle++)
    {
        while (!dummy_data_event.GetNewSamples((
            [](SamplePtr<DummyBenchmarkData> sample) noexcept {
                std::ignore = sample;
            }),
            1).has_value()) {};

        skeleton.dummy_benchmark_data_.Send(std::move(skeleton.dummy_benchmark_data_.Allocate()).value());

    }
    benchmark_finish_point.arrive_and_wait();
    dummy_data_event.Unsubscribe();
    deinit_sync_point.arrive_and_wait();
    skeleton.StopOfferService();
}

int main()
{
    std::cout << "Starting benchmark" << std::endl;

    if (!instance_specifier_resultA2B.has_value() || !instance_specifier_resultB2A.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;
        return EXIT_FAILURE;
    }
    const auto& instance_specifierA2B = instance_specifier_resultA2B.value();
    const auto& instance_specifierB2A = instance_specifier_resultB2A.value();

    std::thread transmitterA(Transmitter, true, std::ref(instance_specifierA2B), std::ref(instance_specifierB2A));
    std::thread transmitterB(Transmitter, false, std::ref(instance_specifierB2A), std::ref(instance_specifierA2B));

    init_sync_point.arrive_and_wait();
    const auto benchmark_start_time = std::chrono::steady_clock::now();
    benchmark_start_point.arrive_and_wait();
    benchmark_finish_point.arrive_and_wait();
    const auto benchmark_stop_time = std::chrono::steady_clock::now();
    const auto benchmark_time = benchmark_stop_time - benchmark_start_time;

    transmitterA.join();
    transmitterB.join();

    std::cout << "Benchmark ::: " <<
        "Iterations: " << kIterations << ","
        "Time: " << duration<float>(benchmark_time).count()  << " s, " <<
        "Latency: " << duration_cast<nanoseconds>(benchmark_time).count() / kIterations << " ns, " << 
        "Sample Size: " << kSampleSize << std::endl;
}
