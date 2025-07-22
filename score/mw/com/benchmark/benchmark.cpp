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

std::mutex cout_mutex{};
score::cpp::latch benchmark_ab_start_point{3}, benchmark_ab_finish_point{3}, init_ab_sync_point{3}, deinit_ab_sync_point{2};
score::cpp::latch benchmark_multi_start_point{1 + kThreadsMultiTotal}, benchmark_multi_finish_point{1 + kThreadsMultiTotal}, init_multi_sync_point{kThreadsMultiTotal}, deinit_multi_sync_point{kThreadsMultiTotal};
const auto instance_specifier_skeleton_ab_a_optional = InstanceSpecifier::Create("benchmark/SkeletonA");
const auto instance_specifier_skeleton_ab_b_optional = InstanceSpecifier::Create("benchmark/SkeletonB");
const auto instance_specifier_skeleton_multi_a_optional = InstanceSpecifier::Create("benchmark/SkeletonMultiA");
const auto instance_specifier_skeleton_multi_b_optional = InstanceSpecifier::Create("benchmark/SkeletonMultiB");

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

void Transmitter(int cpu, bool starter, impl::InstanceSpecifier skeleton_instance_specifier, impl::InstanceSpecifier proxy_instance_specifier)
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

    auto dummy_data_event_optional = GetBenchmarkDataProxyEvent(proxy);
    if (!dummy_data_event_optional.has_value())
    {
        std::cerr << "Could not get dummy_data proxy event" << std::endl;
        return;
    }
    impl::ProxyEvent<score::mw::com::DummyBenchmarkData>& dummy_data_event = dummy_data_event_optional.value().get();

    dummy_data_event.Subscribe(1);

    init_ab_sync_point.arrive_and_wait();
    benchmark_ab_start_point.arrive_and_wait();
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
    benchmark_ab_finish_point.arrive_and_wait();
    dummy_data_event.Unsubscribe();
    deinit_ab_sync_point.arrive_and_wait();
    skeleton.StopOfferService();
}

void Listener(int cpu, impl::InstanceSpecifier proxy_instance_specifier)
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

    auto dummy_data_event_optional = GetBenchmarkDataProxyEvent(proxy);
    if (!dummy_data_event_optional.has_value())
    {
        std::cerr << "Could not get dummy_data proxy event" << std::endl;
        return;
    }
    impl::ProxyEvent<score::mw::com::DummyBenchmarkData>& dummy_data_event = dummy_data_event_optional.value().get();

    dummy_data_event.Subscribe(1);
    init_multi_sync_point.arrive_and_wait();
    benchmark_multi_start_point.arrive_and_wait();
    for (std::size_t cycle = 0U; cycle < kIterations; cycle++)
    {
        while (!dummy_data_event.GetNewSamples((
            [](SamplePtr<DummyBenchmarkData> sample) noexcept {
                std::ignore = sample;
            }),
            1).has_value()) {};

    }
    benchmark_multi_finish_point.arrive_and_wait();
    dummy_data_event.Unsubscribe();
    deinit_multi_sync_point.arrive_and_wait();
}

void Producer(int cpu, impl::InstanceSpecifier skeleton_instance_specifier)
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
    init_multi_sync_point.arrive_and_wait();
    benchmark_multi_start_point.arrive_and_wait();
    for (std::size_t cycle = 0U; cycle < kIterations; cycle++)
    {
        skeleton.dummy_benchmark_data_.Send(std::move(skeleton.dummy_benchmark_data_.Allocate()).value());
    }
    benchmark_multi_finish_point.arrive_and_wait();
    deinit_multi_sync_point.arrive_and_wait();
    skeleton.StopOfferService();
}

int main()
{
    int cpu = 0;

    if (!instance_specifier_skeleton_ab_a_optional.has_value() || !instance_specifier_skeleton_ab_b_optional.has_value()
        || !instance_specifier_skeleton_multi_a_optional.has_value() || !instance_specifier_skeleton_multi_b_optional.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;
        return EXIT_FAILURE;
    }
    const auto& instance_specifier_skeleton_ab_a = instance_specifier_skeleton_ab_a_optional.value();
    const auto& instance_specifier_skeleton_ab_b = instance_specifier_skeleton_ab_b_optional.value();
    const auto& instance_specifier_skeleton_multi_a = instance_specifier_skeleton_multi_a_optional.value();
    const auto& instance_specifier_skeleton_multi_b = instance_specifier_skeleton_multi_b_optional.value();

    std::cout << "Starting A<>B benchmark" << std::endl;

    std::thread transmitterA(Transmitter, cpu++, true, std::ref(instance_specifier_skeleton_ab_a), std::ref(instance_specifier_skeleton_ab_b));
    std::thread transmitterB(Transmitter, cpu++, false, std::ref(instance_specifier_skeleton_ab_b), std::ref(instance_specifier_skeleton_ab_a));

    init_ab_sync_point.arrive_and_wait();
    const auto benchmark_ab_start_time = std::chrono::steady_clock::now();
    benchmark_ab_start_point.arrive_and_wait();
    benchmark_ab_finish_point.arrive_and_wait();
    const auto benchmark_ab_stop_time = std::chrono::steady_clock::now();
    const auto benchmark_ab_time = benchmark_ab_stop_time - benchmark_ab_start_time;

    transmitterA.join();
    transmitterB.join();

    std::cout << "A<>B benchmark"  << "\t" <<
        "Iterations: " << kIterations << ", " << "\t" <<
        "Time: " << duration<float>(benchmark_ab_time).count()  << "s, "  << "\t" <<
        "Latency: " << duration_cast<nanoseconds>(benchmark_ab_time).count() / kIterations << "ns, "  << "\t" <<
        "Sample Size: " << kSampleSize << std::endl;

    std::cout << "Starting multi benchmark" << std::endl;
    
    cpu = 0;
    std::thread subscribers[kSubscribersTotal];
    std::thread producerA, producerB;
    if (kSubscribersA > 0)
    {
        producerA = std::thread(Producer, cpu++, std::ref(instance_specifier_skeleton_multi_a));
    }
    if (kSubscribersB > 0)
    {
        producerB = std::thread(Producer, cpu++, std::ref(instance_specifier_skeleton_multi_b));
    }
    for (std::size_t i = 0; i < kSubscribersA; i++)
    {
        subscribers[i] = std::thread(Listener, cpu++, std::ref(instance_specifier_skeleton_multi_a));
    }

    for (std::size_t i = kSubscribersA; i < kSubscribersTotal; i++)
    {
        subscribers[i] = std::thread(Listener, cpu++, std::ref(instance_specifier_skeleton_multi_b));
    }

    init_multi_sync_point.arrive_and_wait();
    const auto benchmark_multi_start_time = std::chrono::steady_clock::now();
    benchmark_multi_start_point.arrive_and_wait();
    benchmark_multi_finish_point.arrive_and_wait();
    const auto benchmark_multi_stop_time = std::chrono::steady_clock::now();
    const auto benchmark_multi_time = benchmark_multi_stop_time - benchmark_multi_start_time;


    std::cout << "Multi benchmark" << "\t" <<
        "Iterations: " << kIterations << ", " << "\t" <<
        "Time: " << duration<float>(benchmark_multi_time).count()  << "s, " << "\t" <<
        "Latency: " << duration_cast<nanoseconds>(benchmark_multi_time).count() / kIterations << "ns, "  << "\t" <<
        "Sample Size: " << kSampleSize << std::endl;
    
    for (std::size_t i = 0; i < kSubscribersTotal; i++)
    {
        subscribers[i].join();
    }
    
    if (kSubscribersA > 0)
    {
        producerA.join();
    }
    if (kSubscribersB > 0)
    {
        producerB.join();
    }
}
