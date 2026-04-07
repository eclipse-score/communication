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
#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"
#include "score/mw/com/test/partial_restart/methods/test_method_datatype.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <signal.h>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace score::mw::com::test
{
namespace
{

const std::chrono::seconds kMaxWaitTimeToReachCheckpoint{30U};
const std::string_view kShmConsumerCheckpointControlFileName = "methods_consumer_method_reenable_checkpoint_file";
const std::string_view kShmProviderCheckpointControlFileName = "methods_provider_method_reenable_checkpoint_file";
const std::string_view kConsumerCheckpointControlName = "Consumer";
const std::string_view kProviderCheckpointControlName = "Provider";

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"partial_restart/methods/consumer_method_reenable"}).value();

constexpr std::uint8_t kCheckpointMethodCallsSucceeded{1U};

void DoProviderActions(CheckPointControl& check_point_control, score::cpp::stop_token test_stop_token) noexcept
{
    auto skeleton_result = TestMethodServiceSkeleton::Create(kInstanceSpecifier);
    if (!skeleton_result.has_value())
    {
        std::cerr << "Provider: Failed to create skeleton: " << skeleton_result.error() << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    auto skeleton = std::make_unique<TestMethodServiceSkeleton>(std::move(skeleton_result).value());

    auto handler = [](std::int32_t a, std::int32_t b) -> std::int32_t {
        return a + b;
    };

    auto register_result = skeleton->test_method_.RegisterHandler(std::move(handler));
    if (!register_result.has_value())
    {
        std::cerr << "Provider: Failed to register handler: " << register_result.error() << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }

    auto offer_result = skeleton->OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Provider: Failed to offer service: " << offer_result.error() << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }

    std::cerr << "Provider: Service offered, checkpoint (1) reached" << std::endl;
    check_point_control.CheckPointReached(kCheckpointMethodCallsSucceeded);

    // Wait for test completion
    auto wait_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        check_point_control.ErrorOccurred();
        return;
    }

    skeleton->StopOfferService();
}

void DoConsumerActions(CheckPointControl& check_point_control,
                       score::cpp::stop_token test_stop_token,
                       bool enable_methods) noexcept
{
    auto find_result = TestMethodServiceProxy::FindService(kInstanceSpecifier);
    if (!find_result.has_value() || find_result->size() != 1)
    {
        std::cerr << "Consumer: FindService failed" << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }

    if (enable_methods)
    {
        // First consumer OR fixed re-enablement: create proxy with methods
        auto proxy_result = TestMethodServiceProxy::Create((*find_result)[0], {"test_method"});
        if (!proxy_result.has_value())
        {
            std::cerr << "Consumer: Failed to create proxy with methods: " << proxy_result.error() << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
        auto proxy = std::make_unique<TestMethodServiceProxy>(std::move(proxy_result).value());

        // Call method 5 times
        for (std::int32_t method_call_it = 0; method_call_it < 5; ++method_call_it)
        {
            std::int32_t a = method_call_it;
            std::int32_t b = method_call_it * 2;
            auto result = proxy->test_method_(a, b);
            if (!result.has_value())
            {
                std::cerr << "Consumer: Method call " << method_call_it << " failed: " << result.error() << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
            if (*result.value() != (a + b))
            {
                std::cerr << "Consumer: Wrong result for call " << method_call_it << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
        }
        std::cerr << "Consumer: All method calls succeeded" << std::endl;
    }
    else
    {
        // No methods - just verify proxy creation works
        auto proxy_result = TestMethodServiceProxy::Create((*find_result)[0]);
        if (!proxy_result.has_value())
        {
            std::cerr << "Consumer: Failed to create proxy: " << proxy_result.error() << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
        std::cerr << "Consumer: Proxy created (no methods)" << std::endl;
    }

    check_point_control.CheckPointReached(kCheckpointMethodCallsSucceeded);

    auto wait_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        check_point_control.ErrorOccurred();
        return;
    }
}

}  // namespace
}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    using namespace score::mw::com::test;

    score::mw::com::runtime::InitializeRuntime(argc, argv);

    std::cerr << "\n=== Test: Consumer restart with method re-enablement ===" << std::endl;

    ObjectCleanupGuard object_cleanup_guard{};
    score::cpp::stop_source stop_source;
    auto test_stop_token = stop_source.get_token();

    // Create provider checkpoint control
    auto provider_checkpoint_result =
        CreateSharedCheckPointControl("Main", kShmProviderCheckpointControlFileName, kProviderCheckpointControlName);
    if (!provider_checkpoint_result.has_value())
    {
        return EXIT_FAILURE;
    }
    auto& provider_checkpoint = provider_checkpoint_result.value().GetObject();
    object_cleanup_guard.AddProviderCheckpointControlGuard(provider_checkpoint_result.value());

    // Fork provider
    auto provider_pid = ForkProcessAndRunInChildProcess("Main", "Provider", [&provider_checkpoint, test_stop_token]() {
        DoProviderActions(provider_checkpoint, test_stop_token);
    });
    if (!provider_pid.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkProviderGuard(provider_pid.value());

    TimeoutSupervisor timeout_supervisor{};

    // Wait for provider to offer
    auto notification = provider_checkpoint.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Main", notification, provider_checkpoint, kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Create consumer checkpoint control
    auto consumer_checkpoint_result =
        CreateSharedCheckPointControl("Main", kShmConsumerCheckpointControlFileName, kConsumerCheckpointControlName);
    if (!consumer_checkpoint_result.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    auto& consumer_checkpoint = consumer_checkpoint_result.value().GetObject();
    object_cleanup_guard.AddConsumerCheckpointControlGuard(consumer_checkpoint_result.value());

    // First consumer: with methods
    std::cerr << "Main: Starting first consumer (with methods)" << std::endl;
    auto consumer_pid = ForkProcessAndRunInChildProcess("Main", "Consumer1", [&consumer_checkpoint, test_stop_token]() {
        DoConsumerActions(consumer_checkpoint, test_stop_token, true);
    });
    if (!consumer_pid.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(consumer_pid.value());

    notification = consumer_checkpoint.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Main", notification, consumer_checkpoint, kCheckpointMethodCallsSucceeded))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Kill first consumer
    std::cerr << "Main: Killing first consumer" << std::endl;
    consumer_checkpoint.FinishActions();
    if (!WaitForChildProcessToTerminate("Main", consumer_pid.value(), kMaxWaitTimeToReachCheckpoint))
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Second consumer: ALSO with methods (the re-enablement test)
    std::cerr << "Main: Starting second consumer (attempting method re-enablement)" << std::endl;
    consumer_pid = ForkProcessAndRunInChildProcess("Main", "Consumer2", [&consumer_checkpoint, test_stop_token]() {
        DoConsumerActions(consumer_checkpoint, test_stop_token, true);  // enable_methods=true
    });
    if (!consumer_pid.has_value())
    {
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }
    object_cleanup_guard.AddForkConsumerGuard(consumer_pid.value());

    notification = consumer_checkpoint.WaitForCheckpointReachedOrError(
        kMaxWaitTimeToReachCheckpoint, test_stop_token, timeout_supervisor);
    if (!VerifyCheckpoint("Main", notification, consumer_checkpoint, kCheckpointMethodCallsSucceeded))
    {
        std::cerr << "FAIL: Second consumer could not re-enable methods" << std::endl;
        object_cleanup_guard.CleanUp();
        return EXIT_FAILURE;
    }

    // Cleanup
    consumer_checkpoint.FinishActions();
    provider_checkpoint.FinishActions();
    object_cleanup_guard.CleanUp();

    return EXIT_SUCCESS;
}
