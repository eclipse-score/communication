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

/// \brief Main entry point for the long-running handler test.
/// This test verifies that a handler can continue to execute safely after calling
/// StopFindService on its own discovery operation.
int main()
{
    // 1. Initialize the communication runtime.
    score::mw::com::runtime::RuntimeConfiguration config{"/etc/mw_com_config.json"};
    score::mw::com::runtime::InitializeRuntime(config);
    std::cout << "MAIN: Communication runtime initialized.\n";

    // 2. Create the instance specifier for the service.
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

    // 4. Define the service discovery handler.
    std::promise<void> handler_finished_promise;
    auto find_service_handler =
        [&handler_finished_promise](const score::mw::com::ServiceHandleContainer<IpcBridgeProxy::HandleType>&,
                                     score::mw::com::FindServiceHandle find_handle_in_handler) {
            std::cout << "HANDLER: Service found. Stopping find...\n";
            IpcBridgeProxy::StopFindService(find_handle_in_handler);

            // Continue to do work after stopping. This stresses the "late removal" of the handler.
            std::cout << "HANDLER: StopFindService called. Now sleeping for 1 second...\n";
            std::this_thread::sleep_for(1s);

            std::cout << "HANDLER: Sleep finished. Handler is completing.\n";
            handler_finished_promise.set_value();
        };

    // 5. Start asynchronous service discovery.
    std::cout << "MAIN: Starting to find service asynchronously...\n";
    auto find_handle_result = IpcBridgeProxy::StartFindService(find_service_handler, instance_specifier);
    if (!find_handle_result.has_value())
    {
        std::cerr << "MAIN: Failed to start service discovery: " << find_handle_result.error() << ", bailing!\n";
        test_complete_promise.set_value();
        skeleton_thread.join();
        return EXIT_FAILURE;
    }

    // 6. Wait for the handler to complete its logic.
    handler_finished_promise.get_future().wait();
    std::cout << "MAIN: Test logic complete.\n";

    // 7. Clean up all threads.
    test_complete_promise.set_value();
    skeleton_thread.join();

    std::cout << "MAIN: Test finished successfully.\n";
    return EXIT_SUCCESS;
}