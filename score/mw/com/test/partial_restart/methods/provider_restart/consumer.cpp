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
#include "score/mw/com/test/partial_restart/methods/provider_restart/consumer.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/partial_restart/consumer_handle_notification_data.h"
#include "score/mw/com/test/partial_restart/methods/provider_restart/test_checkpoints.h"
#include "score/mw/com/test/partial_restart/methods/test_method_datatype.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/consumer_resources.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

namespace score::mw::com::test
{

namespace
{
const std::chrono::seconds kMaxHandleNotificationWaitTime{15U};
const auto kProxyInstanceSpecifier =
    score::mw::com::InstanceSpecifier::Create(std::string{"partial_restart/methods/provider_restart"}).value();

// Checkpoint identifiers are defined in test_checkpoints.h

void DoConsumerActionsWithProxy(CheckPointControl& check_point_control,
                                HandleNotificationData& handle_notification_data,
                                score::cpp::stop_token test_stop_token) noexcept
{
    // ********************************************************************************
    // Step (2) - Create Proxy for found service with methods enabled
    // ********************************************************************************
    std::cout << "Consumer Step (2): Create Proxy for found service with methods enabled" << std::endl;
    auto lola_proxy_result = TestMethodServiceProxy::Create(*handle_notification_data.handle);
    if (!(lola_proxy_result.has_value()))
    {
        std::cerr << "Consumer: Unable to create lola proxy with methods:" << lola_proxy_result.error() << "\n";
        check_point_control.ErrorOccurred();
        return;
    }
    auto& method_proxy = lola_proxy_result.value();

    // ********************************************************************************
    // Step (3) - Call method multiple times to verify it works
    // ********************************************************************************
    std::cout << "Consumer Step (3): Call method multiple times to verify it works" << std::endl;
    const std::size_t method_call_count{5U};
    std::vector<std::int32_t> results;

    for (std::size_t call_index = 0U; call_index < method_call_count; ++call_index)
    {
        auto arg1 = static_cast<std::int32_t>(call_index);
        auto arg2 = static_cast<std::int32_t>(call_index * 2);
        std::int32_t expected_result = arg1 + arg2;

        std::cout << "Consumer: Calling method with arguments (" << arg1 << ", " << arg2 << ")" << std::endl;

        auto result = method_proxy.test_method_(arg1, arg2);
        if (!result.has_value())
        {
            FailTest("Consumer Step (3): Method call failed with error: ", result.error());
        }

        std::cout << "Consumer: Method returned: " << *result.value() << " (expected: " << expected_result << ")"
                  << std::endl;

        if (*result.value() != expected_result)
        {
            FailTest(
                "Consumer Step (3): Method result mismatch! Got ", *result.value(), " but expected ", expected_result);
        }

        results.push_back(*result.value());
    }

    // ********************************************************************************
    // Step (4) - Notify to Controller, that checkpoint (1) has been reached
    // ********************************************************************************
    std::cout << "Consumer Step (4): Notify controller that checkpoint 1 has been reached" << std::endl;
    check_point_control.CheckPointReached(kCheckpointMethodCallsSucceeded);

    // ********************************************************************************
    // Step (5) - wait for controller to trigger further steps
    // ********************************************************************************
    std::cout << "Consumer Step (5): Wait for controller to trigger further steps" << std::endl;
    auto wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
    {
        FailTest("Consumer Step (5): Wait for controller trigger failed, expected PROCEED_NEXT_CHECKPOINT but got: ",
                 ToString(wait_for_child_proceed_result));
    }

    // ********************************************************************************
    // Step (6) - Wait for service to disappear (provider stops offering)
    // ********************************************************************************
    std::cout << "Consumer Step (6): Wait for service to disappear" << std::endl;
    WaitTillServiceDisappears(handle_notification_data);

    std::cout << "Consumer: Service disappeared - checkpoint (2) reached!" << std::endl;
    check_point_control.CheckPointReached(kCheckpointServiceStopped);

    // ********************************************************************************
    // Step (7) - wait for controller notification to trigger further steps or finish.
    // ********************************************************************************
    std::cout << "Consumer Step (7): Wait for controller notification to trigger further steps" << std::endl;
    wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
    {
        FailTest(
            "Consumer Step (7): Wait for controller notification failed, expected PROCEED_NEXT_CHECKPOINT but got: ",
            ToString(wait_for_child_proceed_result));
    }

    // ********************************************************************************
    // Step (8) - Wait for service to reappear (provider offers service again)
    // ********************************************************************************
    std::cout << "Consumer Step (8): Wait for service to reappear" << std::endl;
    if (!WaitTillServiceAppears(handle_notification_data, kMaxHandleNotificationWaitTime))
    {
        FailTest(
            "Consumer Step (8): Wait for service to reappear failed because handle was not received in time after "
            "provider restart.");
    }

    std::cerr << "Consumer: Service reappeared - checkpoint (3) reached!" << std::endl;
    check_point_control.CheckPointReached(kCheckpointServiceReappeared);

    // ********************************************************************************
    // Step (9) - wait for controller notification to trigger further steps or finish.
    // ********************************************************************************
    std::cout << "Consumer Step (9): Wait for controller notification to trigger further steps" << std::endl;
    wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
    {
        FailTest(
            "Consumer Step (9): Wait for controller notification failed, expected PROCEED_NEXT_CHECKPOINT but got: ",
            ToString(wait_for_child_proceed_result));
    }

    // ********************************************************************************
    // Step (10) - Call methods again after provider restart to verify reconnection
    // ********************************************************************************
    std::cout << "Consumer Step (10): Call methods again after provider restart" << std::endl;
    for (std::size_t call_index = 0U; call_index < method_call_count;)
    {
        auto arg1 = static_cast<std::int32_t>(call_index + 10);
        auto arg2 = static_cast<std::int32_t>(call_index * 3);
        std::int32_t expected_result = arg1 + arg2;

        std::cout << "Consumer: Calling method after restart with arguments (" << arg1 << ", " << arg2 << ")"
                  << std::endl;

        auto call_method_until_success = [&method_proxy, &arg1, &arg2]() -> auto {
            constexpr size_t call_retry_count{20U};
            // this loop is required since the proxy might not yet be resubscribed to the reoffered service
            std::stringstream error_log{};
            for (std::size_t retry_idx = 0U; retry_idx < call_retry_count; ++retry_idx)
            {
                auto result = method_proxy.test_method_(arg1, arg2);
                if (result.has_value())
                {
                    return result;
                }
                error_log << "Retry " << retry_idx << " failed with error:\n\t" << result.error() << "\n";

                // after failiure, backoff with linearly growing waiting intervals before retrying
                auto idling_time = std::chrono::milliseconds(retry_idx + 1U);
                std::this_thread::sleep_for(idling_time);
            }
            // clang-format off
            FailTest("Consumer Step (10): Method call after restart failed. A retry was attempted ",
                     call_retry_count,
                     " times. ",
                     "Most likely proxy could not reconnect with the service after restart.",
                     " Here is the log of all call errors:\n", error_log.str());
            // clang-format on
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
        };

        auto result = call_method_until_success();

        std::cout << "Consumer: Method after restart returned: " << *result.value() << " (expected: " << expected_result
                  << ")" << std::endl;

        if (*result.value() != expected_result)
        {
            FailTest("Consumer Step (10): Method result mismatch after restart! Got ",
                     *result.value(),
                     " but expected ",
                     expected_result);
        }
        ++call_index;
    }

    // ********************************************************************************
    // Step (11) - Notify controller that all calls after restart succeeded
    // ********************************************************************************
    std::cout << "Consumer Step (11): Notify controller that provider restart succeeded - checkpoint (4) reached!"
              << std::endl;
    check_point_control.CheckPointReached(kCheckpointMethodsCalledAfterRestart);

    // ********************************************************************************
    // Step (12) - Wait for controller notification to finish
    // ********************************************************************************
    std::cout << "Consumer Step (12): Wait for controller notification to finish" << std::endl;
    wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        FailTest(
            "Consumer Step (12): Wait for controller notification to finish failed, expected FINISH_ACTIONS but got: ",
            ToString(wait_for_child_proceed_result));
    }
}

}  // namespace

void DoConsumerActions(CheckPointControl& check_point_control,
                       score::cpp::stop_token test_stop_token,
                       std::string_view service_instance_manifest,
                       const bool with_proxy) noexcept
{
    ExitFunctionGuard check_point_control_error_guard{[&check_point_control]() {
        check_point_control.ErrorOccurred();
    }};

    // Initialize mw::com runtime explicitly, if we were called with cmd-line args from main/parent
    std::cout << "Consumer: Initializing LoLa/mw::com runtime from passed service-instance manifest ..." << std::endl;
    mw::com::runtime::InitializeRuntime(mw::com::runtime::RuntimeConfiguration{std::string{service_instance_manifest}});

    HandleNotificationData handle_notification_data{};

    // ********************************************************************************
    // Step (1) - Start an async FindService Search
    // ********************************************************************************
    std::cout << "Consumer Step (1): Start an async FindService search" << std::endl;
    auto find_service_callback = [&handle_notification_data, &check_point_control](auto service_handle_container,
                                                                                   auto) noexcept {
        HandleReceivedNotification(service_handle_container, handle_notification_data, check_point_control);
    };

    auto find_service_handle_result = StartFindService<TestMethodServiceProxy>(
        "Consumer", find_service_callback, kProxyInstanceSpecifier, check_point_control);
    if (!find_service_handle_result.has_value())
    {
        FailTest("Consumer Step (1): Start async FindService search failed.");
    }

    // Wait until Service Discovery returns a valid handle to create the Proxy
    if (!WaitTillServiceAppears(handle_notification_data, kMaxHandleNotificationWaitTime))
    {
        FailTest("Consumer Step (1): Did not receive handle in time!");
    }

    if (with_proxy)
    {
        DoConsumerActionsWithProxy(check_point_control, handle_notification_data, test_stop_token);
    }
    else
    {
        // ********************************************************************************
        // Step (2) - Notify controller that checkpoint (1) has been reached
        // ********************************************************************************
        std::cout << "Consumer Step (2): Service found, notify controller that checkpoint (1) was reached" << std::endl;
        check_point_control.CheckPointReached(kCheckpointMethodCallsSucceeded);

        // ********************************************************************************
        // Step (3) - Wait for controller trigger to proceed
        // ********************************************************************************
        std::cout << "Consumer Step (3): Wait for controller trigger to proceed" << std::endl;
        auto wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
        {
            FailTest(
                "Consumer Step (3): Wait for controller trigger to proceed failed, expected PROCEED_NEXT_CHECKPOINT "
                "but got: ",
                ToString(wait_for_child_proceed_result));
        }

        // ********************************************************************************
        // Step (4) - Wait for service to disappear
        // ********************************************************************************
        std::cout << "Consumer Step (4): Wait for service to disappear" << std::endl;
        WaitTillServiceDisappears(handle_notification_data);

        // ********************************************************************************
        // Step (5) - Notify controller that checkpoint (2) has been reached
        // ********************************************************************************
        std::cout << "Consumer Step (5): Service disappeared, notify controller that checkpoint (2) was reached"
                  << std::endl;
        check_point_control.CheckPointReached(kCheckpointServiceStopped);

        // ********************************************************************************
        // Step (6) - Wait for controller trigger to proceed
        // ********************************************************************************
        std::cout << "Consumer Step (6): Wait for controller trigger to proceed" << std::endl;
        wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
        {
            FailTest(
                "Consumer Step (6): Wait for controller trigger to proceed failed, expected PROCEED_NEXT_CHECKPOINT "
                "but got: ",
                ToString(wait_for_child_proceed_result));
        }

        // ********************************************************************************
        // Step (7) - Wait for service to reappear
        // ********************************************************************************
        std::cout << "Consumer Step (7): Wait for service to reappear" << std::endl;
        if (!WaitTillServiceAppears(handle_notification_data, kMaxHandleNotificationWaitTime))
        {
            FailTest(
                "Consumer Step (7): Wait for service to reappear failed because handle was not received in time after "
                "provider restart.");
        }

        // ********************************************************************************
        // Step (8) - Notify controller that checkpoint (3) has been reached
        // ********************************************************************************
        std::cout << "Consumer Step (8): Service reappeared, notify controller that checkpoint (3) was reached"
                  << std::endl;
        check_point_control.CheckPointReached(kCheckpointServiceReappeared);

        // ********************************************************************************
        // Step (9) - Wait for controller notification to finish
        // ********************************************************************************
        std::cout << "Consumer Step (9): Wait for controller notification to finish" << std::endl;
        wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
        {
            FailTest(
                "Consumer Step (9): Wait for controller notification to finish failed, expected FINISH_ACTIONS but "
                "got: ",
                ToString(wait_for_child_proceed_result));
        }
    }

    const auto stop_find_service_step_idx = with_proxy ? 13 : 10;
    // ********************************************************************************
    // Step (13: with proxy, 10: without proxy) - Stop async FindService
    // ********************************************************************************
    std::cout << "Consumer Step (" << stop_find_service_step_idx << "): Stop async FindService" << std::endl;
    auto stop_find_service_result = TestMethodServiceProxy::StopFindService(find_service_handle_result.value());
    if (!stop_find_service_result.has_value())
    {
        FailTest("Consumer Step (",
                 stop_find_service_step_idx,
                 "): Stop async FindService failed with error: ",
                 stop_find_service_result.error());
    }

    check_point_control_error_guard.Release();

    // ********************************************************************************
    // Step (14: with proxy, 11: without proxy) - Finish actions
    // ********************************************************************************
    const auto finish_actions_step_idx = with_proxy ? 14 : 11;
    std::cout << "Consumer Step (" << finish_actions_step_idx << "): Finish actions" << std::endl;
}

}  // namespace score::mw::com::test
