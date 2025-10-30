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

score::cpp::latch benchmark_ab_start_point{3}, benchmark_ab_finish_point{3}, init_ab_sync_point{3}, deinit_ab_sync_point{2};

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

void Transceiver(int cpu, bool starter, impl::InstanceSpecifier skeleton_instance_specifier, impl::InstanceSpecifier proxy_instance_specifier)
{
    SetupThread(cpu);

    auto create_result = BenchmarkSkeleton::Create(skeleton_instance_specifier);
    if (!create_result.has_value())
    {
        std::cerr << "Unable to construct skeleton: " << create_result.error() << "!" << std::endl;
        return;
    }
    auto& skeleton = create_result.value();
    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer service for skeleton: " << offer_result.error() << "!" << std::endl;
        return;
    }

    ServiceHandleContainer<impl::HandleType> handle{};
    do
    {
        auto handles_result = BenchmarkProxy::FindService(proxy_instance_specifier);
        if (!handles_result.has_value())
        {
            std::cerr << "Unable to find service: " << handles_result.error() << "!" << std::endl;
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
        std::cerr << "Unable to construct proxy: " << proxy_result.error() << "!" << std::endl;
        return;
    }
    auto& proxy = proxy_result.value();

    impl::ProxyEvent<score::mw::com::DummyBenchmarkData>& dummy_data_event = proxy.dummy_benchmark_data_;
    score::Result<score::mw::com::impl::SampleAllocateePtr<score::mw::com::DummyBenchmarkData>> sample_result;

    dummy_data_event.Subscribe(1);

    init_ab_sync_point.arrive_and_wait();
    benchmark_ab_start_point.arrive_and_wait();
    if (starter)
    {
        do {
            sample_result = skeleton.dummy_benchmark_data_.Allocate();
        } while (!sample_result.has_value());
        skeleton.dummy_benchmark_data_.Send(std::move(sample_result).value());
    }
    for (std::size_t cycle = 0U; cycle < kIterations; cycle++)
    {
        while (true) {
            auto result = dummy_data_event.GetNewSamples((
            [](SamplePtr<DummyBenchmarkData> sample) noexcept {
                std::ignore = sample;
            }),1);
            if (result.has_value())
            {
                if (result.value() == 0)
                {
                    continue;
                } else {
                    break;
                }
            }
        };
        do {
            sample_result = skeleton.dummy_benchmark_data_.Allocate();
        } while (!sample_result.has_value());
        skeleton.dummy_benchmark_data_.Send(std::move(sample_result).value());

    }
    benchmark_ab_finish_point.arrive_and_wait();
    dummy_data_event.Unsubscribe();
    deinit_ab_sync_point.arrive_and_wait();
    skeleton.StopOfferService();
}

void Subscriber(int cpu, impl::InstanceSpecifier proxy_instance_specifier)
{
    SetupThread(cpu);

    ServiceHandleContainer<impl::HandleType> handle{};
    do
    {
        auto handles_result = BenchmarkProxy::FindService(proxy_instance_specifier);
        if (!handles_result.has_value())
        {
            std::cerr << "Unable to find service: " << handles_result.error() << "!" << std::endl;
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
        std::cerr << "Unable to construct proxy: " << proxy_result.error() << "!" << std::endl;
        return;
    }
    auto& proxy = proxy_result.value();

    impl::ProxyEvent<score::mw::com::DummyBenchmarkData>& dummy_data_event = proxy.dummy_benchmark_data_;

    dummy_data_event.Subscribe(1);
    for (std::size_t cycle = 0U; cycle < kIterations; cycle++)
    {
        while (true) {
            auto result = dummy_data_event.GetNewSamples((
            [](SamplePtr<DummyBenchmarkData> sample) noexcept {
                std::ignore = sample;
            }),1);
            if (result.has_value())
            {
                if (result.value() == 0)
                {
                    continue;
                } else {
                    break;
                }
            }
        };
    }
    dummy_data_event.Unsubscribe();
}

int main()
{
    int cpu = 0;
    const auto instance_specifier_instance_a_result = InstanceSpecifier::Create("benchmark/InstanceA");
    const auto instance_specifier_instance_b_result = InstanceSpecifier::Create("benchmark/InstanceB");

    if (!instance_specifier_instance_a_result.has_value() || !instance_specifier_instance_b_result.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;
        return EXIT_FAILURE;
    }
    const auto& instance_specifier_instance_a = instance_specifier_instance_a_result.value();
    const auto& instance_specifier_instance_b = instance_specifier_instance_b_result.value();

    std::cout << "Starting benchmark" << std::endl;

    std::thread transceiverA(Transceiver, cpu++, true, std::ref(instance_specifier_instance_a), std::ref(instance_specifier_instance_b));
    std::thread transceiverB(Transceiver, cpu++, false, std::ref(instance_specifier_instance_b), std::ref(instance_specifier_instance_a));
#if kSubscribers > 0
    std::thread subscribers[kSubscribers];
    if (kSubscribers > 0)
    {
        subscribers[i] = std::thread(Subscriber, cpu++, std::ref(instance_specifier_instance_a));
        subscribers[i] = std::thread(Subscriber, cpu++, std::ref(instance_specifier_instance_b));
    }
#endif
    init_ab_sync_point.arrive_and_wait();
    benchmark_ab_start_point.arrive_and_wait();
    const auto benchmark_ab_start_time = std::chrono::steady_clock::now();
    benchmark_ab_finish_point.arrive_and_wait();
    const auto benchmark_ab_stop_time = std::chrono::steady_clock::now();
    const auto benchmark_ab_time = benchmark_ab_stop_time - benchmark_ab_start_time;

    transceiverA.join();
    transceiverB.join();
#if kSubscribers > 0
    for (std::size_t i = 0; i < kSubscribers; i++)
    {
        subscribers[i].join();
    }
#endif
    std::cout << "Results:"  << "\t" <<
        "Iterations: " << kIterations << ", " << "\t" <<
        "Time: " << duration<float>(benchmark_ab_time).count()  << "s, "  << "\t" <<
        "Latency: " << duration_cast<nanoseconds>(benchmark_ab_time).count() / (kIterations * 2) << "ns, "  << "\t" <<
        "Sample Size: " << kSampleSize << 
        "Additional subscribers: " << kSubscribers << std::endl;
}
