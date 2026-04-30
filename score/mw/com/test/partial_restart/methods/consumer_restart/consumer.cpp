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
#include "score/mw/com/test/partial_restart/methods/consumer_restart/consumer.h"
#include "score/mw/com/test/partial_restart/methods/consumer_handle_notification_data.h"
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
#include <mutex>
#include <thread>
#include <vector>

namespace score::mw::com::test
{

namespace
{
const std::chrono::seconds kMaxHandleNotificationWaitTime{15U};
constexpr std::uint8_t kCheckpointMethodCallsSucceeded{1U};
}  // namespace

void DoConsumerActions(CheckPointControl& check_point_control,
                       score::cpp::stop_token test_stop_token,
                       int argc,
                       const char** argv,
                       bool enable_methods) noexcept
{
    // Initialize mw::com runtime explicitly, if we were called with cmd-line args from main/parent
    if (argc > 0 && argv != nullptr)
    {
        std::cerr
            << "Consumer: Initializing LoLa/mw::com runtime from cmd-line args handed over by parent/controller ..."
            << std::endl;
        mw::com::runtime::InitializeRuntime(argc, argv);
        std::cerr << "Consumer: Initializing LoLa/mw::com runtime done." << std::endl;
    }

    HandleNotificationData handle_notification_data{};

    // ********************************************************************************
    // Step (1) - Start an async FindService Search
    // ********************************************************************************
    auto instance_specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"partial_restart/methods/consumer_restart"});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Consumer: Could not create instance specifier due to error "
                  << std::move(instance_specifier_result).error() << ", terminating!\n";
        check_point_control.ErrorOccurred();
        return;
    }

    auto find_service_callback = [&handle_notification_data, &check_point_control](auto service_handle_container,
                                                                                   auto) noexcept {
        HandleReceivedNotification(service_handle_container, handle_notification_data, check_point_control);
    };

    auto find_service_handle_result = StartFindService<TestMethodServiceProxy>(
        "Consumer", find_service_callback, instance_specifier_result.value(), check_point_control);
    if (!find_service_handle_result.has_value())
    {
        return;
    }

    // Wait until Service Discovery returns a valid handle to create the Proxy
    if (!WaitTillServiceAppears(handle_notification_data, kMaxHandleNotificationWaitTime))
    {
        std::cerr << "Consumer Step (1): Did not receive handle in time!" << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }

    // ********************************************************************************
    // Step (2) - Create Proxy for found service
    // ********************************************************************************
    if (enable_methods)
    {
        // Create proxy with methods enabled - full method test
        auto lola_proxy_result = TestMethodServiceProxy::Create(*handle_notification_data.handle, {"test_method"});
        if (!(lola_proxy_result.has_value()))
        {
            std::cerr << "Consumer: Unable to create lola proxy:" << lola_proxy_result.error() << "\n";
            check_point_control.ErrorOccurred();
            return;
        }
        std::cerr << "Consumer: Successfully created lola proxy (with methods)" << std::endl;
        auto& method_proxy = lola_proxy_result.value();

        // ********************************************************************************
        // Step (3) - Call method multiple times to verify it works
        // ********************************************************************************
        const std::size_t method_call_count{5U};

        for (std::size_t call_index = 0U; call_index < method_call_count; ++call_index)
        {
            std::int32_t arg1 = static_cast<std::int32_t>(call_index);
            std::int32_t arg2 = static_cast<std::int32_t>(call_index * 2);
            std::int32_t expected_result = arg1 + arg2;

            std::cout << "Consumer: Calling method with arguments (" << arg1 << ", " << arg2 << ")" << std::endl;

            auto result = method_proxy.test_method_(arg1, arg2);
            if (!result.has_value())
            {
                std::cerr << "Consumer: Method call failed with error: " << result.error().Message() << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }

            std::cout << "Consumer: Method returned: " << *result.value() << " (expected: " << expected_result << ")"
                      << std::endl;

            if (*result.value() != expected_result)
            {
                std::cerr << "Consumer: Method result mismatch! Got " << *result.value() << " but expected "
                          << expected_result << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
        }

        std::cerr << "Consumer: All method calls succeeded - checkpoint (1) reached!" << std::endl;
    }
    else
    {
        // After consumer restart: create proxy WITHOUT methods enabled.
        // This verifies that FindService and proxy creation work after consumer restart.
        auto lola_proxy_result = TestMethodServiceProxy::Create(*handle_notification_data.handle);
        if (!(lola_proxy_result.has_value()))
        {
            std::cerr << "Consumer: Unable to create lola proxy (reconnect):" << lola_proxy_result.error() << "\n";
            check_point_control.ErrorOccurred();
            return;
        }
        std::cerr << "Consumer: Successfully created lola proxy (reconnect, without methods)" << std::endl;
        std::cerr << "Consumer: Reconnection verified - checkpoint (1) reached!" << std::endl;
    }

    check_point_control.CheckPointReached(kCheckpointMethodCallsSucceeded);

    // ********************************************************************************
    // Step (5) - Wait for controller notification to finish
    // ********************************************************************************
    auto wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
    if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        std::cerr << "Consumer: Expected to get notification to finish gracefully but got: "
                  << static_cast<std::uint8_t>(wait_for_child_proceed_result) << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }

    std::cerr << "Consumer: Exiting gracefully." << std::endl;
}

}  // namespace score::mw::com::test
