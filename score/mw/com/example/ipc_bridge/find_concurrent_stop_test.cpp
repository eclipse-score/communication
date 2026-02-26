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

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using IpcBridgeProxy = score::mw::com::AsProxy<score::mw::com::IpcBridgeInterface>;

/// \brief Runs the service provider (skeleton) logic.
void RunSkeleton(const score::mw::com::InstanceSpecifier& instance_specifier, std::future<void> test_complete_future)
{
    auto create_result = score::mw::com::AsSkeleton<score::mw::com::IpcBridgeInterface>::Create(instance_specifier);
    if (!create_result.has_value())
    {
        std::cerr << "SKELETON: Unable to construct skeleton: " << create_result.error() << ", bailing!\n";
        return;
    }
    auto& skeleton = create_result.value();

    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "SKELETON: Unable to offer service: " << offer_result.error() << ", bailing!\n";
        return;
    }
    std::cout << "SKELETON: Service offered successfully.\n";

    // Wait until the main test logic is complete.
    test_complete_future.wait();

    std::cout << "SKELETON: Stopping offer service...\n";
    skeleton.StopOfferService();
    std::cout << "SKELETON: Terminating.\n";
}

/// \brief Main entry point for the concurrent stop test.
/// This test verifies that calling StopFindService for the same handle from two threads
/// concurrently is handled gracefully.
int main()
{
    // 1. Initialize the communication runtime.
    score::mw::com::runtime::RuntimeConfiguration config{"/etc/mw_com_config.json"};
    score::mw::com::runtime::InitializeRuntime(config);
    std::cout << "MAIN: Communication runtime initialized.\n";

    // 2. Create the instance specifier for the service we want to test.
    const auto instance_specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"xpad/cp60/MapApiLanesStamped"});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "MAIN: Invalid instance specifier: " << instance_specifier_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    const auto& instance_specifier = instance_specifier_result.value();

    // 3. Start the skeleton in a background thread.
    std::promise<void> test_complete_promise;
    auto test_complete_future = test_complete_promise.get_future();
    std::thread skeleton_thread(RunSkeleton, std::cref(instance_specifier), std::move(test_complete_future));

    // 4. Set up synchronization primitives for the concurrent stop calls.
    std::promise<void> handler_finished_promise;
    auto handler_finished_future = handler_finished_promise.get_future();
    std::promise<void> release_stopper_promise;
    auto release_stopper_future = release_stopper_promise.get_future();

    // 5. Define the service discovery handler.
    auto find_service_handler =
        [&release_stopper_promise, &handler_finished_promise](
            const score::mw::com::ServiceHandleContainer<IpcBridgeProxy::HandleType>&,
            score::mw::com::FindServiceHandle find_handle_in_handler) {
            std::cout << "HANDLER: Service found. Releasing stopper thread and stopping find...\n";
            // Release the stopper thread to create a race.
            release_stopper_promise.set_value();
            // The handler also tries to stop.
            auto result = IpcBridgeProxy::StopFindService(find_handle_in_handler);
            std::cout << "HANDLER: StopFindService called with result: "
                      << (result.has_value() ? "success" : result.error().Message()) << "\n";
            handler_finished_promise.set_value();
        };

    // 6. Start asynchronous service discovery.
    std::cout << "MAIN: Starting to find service asynchronously...\n";
    auto find_handle_result = IpcBridgeProxy::StartFindService(find_service_handler, instance_specifier);
    if (!find_handle_result.has_value())
    {
        std::cerr << "MAIN: Failed to start service discovery: " << find_handle_result.error() << ", bailing!\n";
        test_complete_promise.set_value();
        skeleton_thread.join();
        return EXIT_FAILURE;
    }
    auto find_handle = find_handle_result.value();

    // 7. Create and start the "stopper" thread.
    std::thread stopper_thread([&release_stopper_future, find_handle]() {
        std::cout << "STOPPER: Waiting for release signal from handler...\n";
        release_stopper_future.wait();
        std::cout << "STOPPER: Released. Calling StopFindService...\n";
        // The stopper thread also tries to stop.
        auto result = IpcBridgeProxy::StopFindService(find_handle);
        std::cout << "STOPPER: StopFindService called with result: "
                  << (result.has_value() ? "success" : result.error().Message()) << "\n";
    });

    // 8. Wait for the handler to complete its logic.
    handler_finished_future.wait();
    std::cout << "MAIN: Test logic complete.\n";

    // 9. Clean up all threads.
    stopper_thread.join();
    test_complete_promise.set_value();
    skeleton_thread.join();

    std::cout << "MAIN: Test finished successfully.\n";
    return EXIT_SUCCESS;
}
