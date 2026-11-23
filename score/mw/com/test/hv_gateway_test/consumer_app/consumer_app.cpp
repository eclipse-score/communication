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

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/big_datatype.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include "score/memory/shared/shared_memory_factory.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_bar_discovery.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_typed_memory_provider.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

namespace
{

constexpr std::size_t kMaxSamples = 2U;
constexpr std::size_t kSuccessThreshold = 3U;
constexpr const char* kSuccessSentinelPath = "/tmp/hv_gateway_test_success";

/// \brief Touch a sentinel file once, signalling to the integration test harness that
/// enough samples were received end-to-end through the gateway. This is written exactly
/// once (guarded by an atomic flag) so it doesn't interfere with the app's normal loop.
void WriteSuccessSentinelOnce(std::atomic<bool>& sentinel_written)
{
    bool expected = false;
    if (sentinel_written.compare_exchange_strong(expected, true))
    {
        std::ofstream sentinel_file(kSuccessSentinelPath);
        std::cout << "[ConsumerApp] Wrote success sentinel to " << kSuccessSentinelPath << "\n";
    }
}

/// \brief Received sample is a "huge" datatype. We jump between array members lane_boundaries, lanes and lane_groups
/// to simulate worst-case memory access patterns for the sample. This is to ensure that the memory access between
/// hostVM and guestVM don't greatly differ!
/// \param sample
void AccessSample(const score::mw::com::test::MapApiLanesStamped* sample, std::vector<long>& run_times)
{
    std::uint64_t sum = 0;
    auto index_jump = score::mw::com::test::MAX_LANES / 100;

    auto start = std::chrono::steady_clock::now();

    for (std::size_t index = 0; index < score::mw::com::test::MAX_LANES; index += index_jump)
    {
        sum += sample->lanes.at(index).left_boundary_id;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    run_times.push_back(duration);
}

}  // namespace

int main(int argc, const char** argv)
{
    // The forwarding skeleton's service instance has "interVmSupport": true, so the proxy must open
    // its shm via the same ivshmem BAR (visible to the gateway/peer VM) instead of ordinary local shm.
    std::uint64_t ivshmem_paddr = 0U;
    std::uint64_t ivshmem_size = 0U;
    if (!score::mw::com::gateway::qemu::ivshmem::DiscoverIvshmemBar(ivshmem_paddr, ivshmem_size))
    {
        std::cerr << "[ConsumerApp] Failed to discover ivshmem BAR\n";
        return EXIT_FAILURE;
    }
    auto ivshmem_provider = std::make_shared<score::mw::com::gateway::qemu::ivshmem::IvshmemTypedMemoryProvider>(
        ivshmem_paddr, ivshmem_size);
    score::memory::shared::SharedMemoryFactory::SetInterVMMemoryProvider(ivshmem_provider);

    score::mw::com::runtime::InitializeRuntime(argc, argv);

    score::cpp::stop_source stop_source;
    score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    const auto stop_token = stop_source.get_token();

    auto specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"hypervisor/poc/TestEventServiceInstance"});
    if (!specifier_result.has_value())
    {
        std::cerr << "[ConsumerApp] Invalid instance specifier\n";
        return EXIT_FAILURE;
    }
    const auto& instance_specifier = specifier_result.value();

    std::cout << "[ConsumerApp] Waiting for service...\n";

    score::mw::com::ServiceHandleContainer<score::mw::com::impl::HandleType> handles{};
    const auto find_deadline = std::chrono::steady_clock::now() + 30s;
    while (handles.empty() && !stop_token.stop_requested())
    {
        if (std::chrono::steady_clock::now() >= find_deadline)
        {
            std::cerr << "[ConsumerApp] TIMEOUT waiting for service discovery\n";
            return EXIT_FAILURE;
        }
        auto handles_result = score::mw::com::test::BigDataProxy::FindService(instance_specifier);
        if (handles_result.has_value())
        {
            handles = std::move(handles_result).value();
        }
        if (handles.empty())
        {
            std::this_thread::sleep_for(100ms);
        }
    }

    if (handles.empty())
    {
        std::cerr << "[ConsumerApp] No service handles found\n";
        return EXIT_FAILURE;
    }

    auto proxy_result = score::mw::com::test::BigDataProxy::Create(handles.front());
    if (!proxy_result.has_value())
    {
        std::cerr << "[ConsumerApp] Failed to create proxy: " << proxy_result.error() << "\n";
        return EXIT_FAILURE;
    }
    auto& proxy = proxy_result.value();

    std::atomic<std::size_t> received_count{0U};
    std::atomic<bool> sentinel_written{false};
    std::vector<long> run_times;

    // Bundle the receive-handler's captured state behind a single pointer: the outer
    // lambda's erased size is bounded (score::cpp::move_only_function has a fixed
    // inline capacity), so capturing several references individually can overflow it.
    struct ReceiveHandlerState
    {
        decltype(proxy)& proxy_ref;
        std::atomic<std::size_t>& received_count_ref;
        std::vector<long>& run_times_ref;
        std::atomic<bool>& sentinel_written_ref;
    } receive_handler_state{proxy, received_count, run_times, sentinel_written};

    auto handler_result = proxy.map_api_lanes_stamped_.SetReceiveHandler([&receive_handler_state]() noexcept {
        auto& state = receive_handler_state;
        const auto get_samples_result = state.proxy_ref.map_api_lanes_stamped_.GetNewSamples(
            [&state](score::mw::com::SamplePtr<score::mw::com::test::MapApiLanesStamped> sample) {
                std::cout << "[ConsumerApp] Received sample value: " << sample->x << "\n";
                ++state.received_count_ref;
                AccessSample(sample.Get(), state.run_times_ref);
            },
            kMaxSamples);
        if (!get_samples_result.has_value())
        {
            std::cerr << "[ConsumerApp] Failed to get new samples: " << get_samples_result.error() << "\n";
        }
        if (state.received_count_ref.load() >= kSuccessThreshold)
        {
            WriteSuccessSentinelOnce(state.sentinel_written_ref);
        }
    });
    if (!handler_result.has_value())
    {
        std::cerr << "[ConsumerApp] Failed to set receive handler: " << handler_result.error() << "\n";
        return EXIT_FAILURE;
    }

    auto subscribe_result = proxy.map_api_lanes_stamped_.Subscribe(kMaxSamples);
    if (!subscribe_result.has_value())
    {
        std::cerr << "[ConsumerApp] Failed to subscribe: " << subscribe_result.error() << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "[ConsumerApp] Subscribed to event\n";

    while (!stop_token.stop_requested())
    {
        std::this_thread::sleep_for(100ms);
    }

    proxy.map_api_lanes_stamped_.Unsubscribe();
    std::cout << "[ConsumerApp] Received " << received_count.load() << " sample(s) through gateway\n";

    return (received_count.load() > 0U) ? EXIT_SUCCESS : EXIT_FAILURE;
}
