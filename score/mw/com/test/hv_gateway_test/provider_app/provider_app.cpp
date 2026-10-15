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
 *******************************************************************************/

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/big_datatype.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include "score/memory/shared/shared_memory_factory.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_bar_discovery.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_typed_memory_provider.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

int main(int argc, const char** argv)
{
    // Our skeleton's service instance has "interVmSupport": true, so its shm must be created
    // in the ivshmem BAR (visible to the gateway/peer VM) instead of ordinary local shm.
    std::uint64_t ivshmem_paddr = 0U;
    std::uint64_t ivshmem_size = 0U;
    if (!score::mw::com::gateway::qemu::ivshmem::DiscoverIvshmemBar(ivshmem_paddr, ivshmem_size))
    {
        std::cerr << "[ProviderApp] Failed to discover ivshmem BAR\n";
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
        std::cerr << "[ProviderApp] Invalid instance specifier\n";
        return EXIT_FAILURE;
    }

    auto skeleton_result = score::mw::com::test::BigDataSkeleton::Create(specifier_result.value());
    if (!skeleton_result.has_value())
    {
        std::cerr << "[ProviderApp] Failed to create skeleton: " << skeleton_result.error() << "\n";
        return EXIT_FAILURE;
    }
    auto& skeleton = skeleton_result.value();

    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "[ProviderApp] Failed to offer service: " << offer_result.error() << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "[ProviderApp] Skeleton offered\n";

    std::uint32_t counter = 1U;
    while (!stop_token.stop_requested())
    {
        auto sample_result = skeleton.map_api_lanes_stamped_.Allocate();
        if (!sample_result.has_value())
        {
            std::cerr << "[ProviderApp] Failed to allocate sample\n";
            std::this_thread::sleep_for(100ms);
            continue;
        }
        auto sample = std::move(sample_result).value();
        sample->x = counter;

        const auto send_result = skeleton.map_api_lanes_stamped_.Send(std::move(sample));
        if (!send_result.has_value())
        {
            std::cerr << "Failure while sending data" << std::endl;
        }
        std::cout << "[ProviderApp] Sent value " << counter << "\n";

        ++counter;
        std::this_thread::sleep_for(100ms);
    }

    skeleton.StopOfferService();
    std::cout << "[ProviderApp] Stopped\n";
    return EXIT_SUCCESS;
}
