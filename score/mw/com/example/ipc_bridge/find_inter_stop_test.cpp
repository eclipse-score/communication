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


#include "datatype.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <score/assert.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using IpcBridgeProxy = score::mw::com::AsProxy<score::mw::com::IpcBridgeInterface>;

/// \brief Runs a skeleton service provider.
void RunSkeleton(const score::mw::com::InstanceSpecifier& instance_specifier, std::atomic<bool>& shutdown_flag)
{
    auto create_result = score::mw::com::AsSkeleton<score::mw::com::IpcBridgeInterface>::Create(instance_specifier);
    if (!create_result.has_value())
    {
        std::cerr << "SKELETON (" << instance_specifier.ToString() << "): Unable to construct skeleton: " << create_result.error() << ", bailing!\n";
        return;
    }
    auto& skeleton = create_result.value();

    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "SKELETON (" << instance_specifier.ToString() << "): Unable to offer service: " << offer_result.error() << ", bailing!\n";
        return;
    }
    std::cout << "SKELETON (" << instance_specifier.ToString() << "): Service offered.\n";

    while (!shutdown_flag)
    {
        std::this_thread::sleep_for(100ms);
    }

    skeleton.StopOfferService();
    std::cout << "SKELETON (" << instance_specifier.ToString() << "): Terminating.\n";
}

/// \brief Main entry point for the inter-handler stop test.
/// This test verifies that calling StopFindService for one discovery operation from
/// within the handler of another discovery operation works correctly.
int main()
{
    // 1. Initialize the communication runtime.
    score::mw::com::runtime::RuntimeConfiguration config{"/etc/mw_com_config.json"};
    score::mw::com::runtime::InitializeRuntime(config);
    std::cout << "MAIN: Communication runtime initialized.\n";

    // 2. Create instance specifiers for two different services.
    const auto spec_A_res = score::mw::com::InstanceSpecifier::Create(std::string{"xpad/cp60/MapApiLanesStamped"});
    if (!spec_A_res.has_value())
    {
        std::cerr << "MAIN: Invalid instance specifier A: " << spec_A_res.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    const auto spec_B_res = score::mw::com::InstanceSpecifier::Create(std::string{"xpad/cp60/MapApiLanesStamped_B"});
    if (!spec_B_res.has_value())
    {
        std::cerr << "MAIN: Invalid instance specifier B: " << spec_B_res.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    const auto& spec_A = spec_A_res.value();
    const auto& spec_B = spec_B_res.value();

    // 3. Start two skeletons in background threads.
    std::atomic<bool> shutdown_flag{false};
    std::thread skeleton_A_thread(RunSkeleton, std::cref(spec_A), std::ref(shutdown_flag));
    std::thread skeleton_B_thread(RunSkeleton, std::cref(spec_B), std::ref(shutdown_flag));

    // 4. Set up synchronization primitives.
    std::promise<void> handler_A_finished_promise;
    auto handler_A_finished_future = handler_A_finished_promise.get_future();

    // We need to capture the handle for discovery B to stop it from handler A.
    // Since the handle is only created after StartFindService, we use a shared_ptr to allow late assignment.
    std::shared_ptr<score::mw::com::FindServiceHandle> find_handle_B_ptr;

    // 5. Define the handlers for each discovery.
    auto find_service_handler_A =
        [&handler_A_finished_promise, find_handle_B_ptr](
            const score::mw::com::ServiceHandleContainer<IpcBridgeProxy::HandleType>&,
            score::mw::com::FindServiceHandle) {
            std::cout << "HANDLER A: Service A found. Stopping discovery for service B...\n";
            // Stop the *other* discovery operation.
            if (find_handle_B_ptr) // Check if the pointer is valid before dereferencing
            {
                auto result = IpcBridgeProxy::StopFindService(*find_handle_B_ptr);
                if (!result.has_value())
                    std::cerr << "HANDLER A: Failed to stop discovery for service B: " << result.error().Message() << "\n";
            }
            handler_A_finished_promise.set_value();
        };

    auto find_service_handler_B = [](const score::mw::com::ServiceHandleContainer<IpcBridgeProxy::HandleType>&,
                                     score::mw::com::FindServiceHandle) {
        // This handler should ideally not be called if A stops it fast enough,
        // but it's not an error if it is.
        std::cout << "HANDLER B: Service B found.\n";
    };

    // 6. Start both asynchronous service discoveries.
    std::cout << "MAIN: Starting discovery for Service A and Service B...\n";
    auto find_handle_A_result = IpcBridgeProxy::StartFindService(find_service_handler_A, spec_A);
    auto find_handle_B_result = IpcBridgeProxy::StartFindService(find_service_handler_B, spec_B);

    if (!find_handle_A_result.has_value())
    {
        std::cerr << "MAIN: Failed to start discovery for A: " << find_handle_A_result.error() << ", bailing!\n";
        shutdown_flag = true;
        skeleton_A_thread.join();
        skeleton_B_thread.join();
        return EXIT_FAILURE;
    }
    if (!find_handle_B_result.has_value())
    {
        std::cerr << "MAIN: Failed to start discovery for B: " << find_handle_B_result.error() << ", bailing!\n";
        IpcBridgeProxy::StopFindService(find_handle_A_result.value());
        shutdown_flag = true;
        skeleton_A_thread.join();
        skeleton_B_thread.join();
        return EXIT_FAILURE;
    }

    find_handle_B_ptr = std::make_shared<score::mw::com::FindServiceHandle>(find_handle_B_result.value());

    // 7. Wait for handler A to complete its logic.
    handler_A_finished_future.wait();
    std::cout << "MAIN: Test logic complete.\n";

    // 8. Clean up all threads.
    // Stop the remaining discovery if it's still active.
    IpcBridgeProxy::StopFindService(find_handle_A_result.value());

    shutdown_flag = true;
    skeleton_A_thread.join();
    skeleton_B_thread.join();

    std::cout << "MAIN: Test finished successfully.\n";
    return EXIT_SUCCESS;
}