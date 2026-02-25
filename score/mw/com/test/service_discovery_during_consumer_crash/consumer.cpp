/*******************************************************************************
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

#include "score/mw/com/test/service_discovery_during_consumer_crash/consumer.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/consumer_resources.h"

#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/service_discovery_during_consumer_crash/test_datatype.h"

#include <iostream>
#include <random>

namespace score::mw::com::test
{
namespace
{

struct HandleNotificationData
{
    std::mutex mutex{};
    std::condition_variable condition_variable{};
    std::unique_ptr<score::mw::com::test::TestServiceProxy::HandleType> handle{nullptr};
};

const auto kProxyInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/service_discovery_during_consumer_crash"}).value();
const std::chrono::seconds kMaxHandleNotificationWaitTime{15U};

}  // namespace

void DoConsumerActionsFirstTime(score::mw::com::test::CheckPointControl& check_point_control,
                                score::cpp::stop_token test_stop_token,
                                int argc,
                                const char** argv)
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

    // ********************************************************************************
    // Step (C.1) - Start an async FindService Search
    // ********************************************************************************
    std::cerr << "Consumer Step (C.1): Call StartFindService" << std::endl;
    // HandleNotificationData handle_notification_data{};
    auto find_service_callback = [&check_point_control]([[maybe_unused]] auto service_handle_container,
                                                        [[maybe_unused]] auto find_service_handle) noexcept {
        std::cerr << "Consumer Step (C.1): find service handler called" << std::endl;
        if (service_handle_container.size() != 1)
        {
            std::cerr
                << "Consumer Step (C.1): Error - StartFindService() is expected to find 1 service instance but found: "
                << service_handle_container.size() << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }

        std::cerr << "Consumer Step (C.1): FindServiceHandler handler done - found one service instance." << std::endl;
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;

        auto a_quick_pause = get_random_time();
        std::cerr << "Consumer: FindService Callback: sleeping for " << a_quick_pause.count() << "ns." << std::endl;
        std::this_thread::sleep_for(a_quick_pause);
        std::cerr << "Consumer: FindService Callback: Finished sleeping." << std::endl;

        if (auto result = TestServiceProxy::StopFindService(find_service_handle); !result.has_value())
        {
            std::cout << "Consumer Step (C.1): Error Occurred during StopFindService." << std::endl;
            std::cout << result.error() << std::endl;
            check_point_control.ErrorOccurred();
        }
        else
        {
            std::cout << "Consumer Step (C.1): StopFindService was called." << std::endl;
        }
    };

    check_point_control.CheckPointReached(1);
    if (WaitForChildProceed(check_point_control, test_stop_token) !=
        CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
    {
        std::cerr << "Provider Step (C.1): Incorrect instruction received." << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }

    auto find_service_handle_result = StartFindService<TestServiceProxy>(
        "Consumer Step (C.1)", find_service_callback, kProxyInstanceSpecifier, check_point_control);
    if (!find_service_handle_result.has_value())
    {
        return;
    }

    std::cerr << "Consumer Step (C.1): waiting to get killed by the controller." << std::endl;
    while (true)
    {
    };
}

void DoConsumerActionsAfterRestart(score::mw::com::test::CheckPointControl& check_point_control,
                                   int argc,
                                   const char** argv)
{
    // Initialize mw::com runtime explicitly, if we were called with cmd-line args from main/parent

    if (argc > 0 && argv != nullptr)
    {
        std::cerr << "Reconnected Consumer: Initializing LoLa/mw::com runtime from cmd-line args handed over by "
                     "parent/controller ..."
                  << std::endl;
        mw::com::runtime::InitializeRuntime(argc, argv);
        std::cerr << "Reconnected Consumer: Initializing LoLa/mw::com runtime done." << std::endl;
    }

    // ********************************************************************************
    // Step (RC.1) - Start an async FindService Search
    // ********************************************************************************
    std::cerr << "Reconnected Consumer Step (RC.1): Call StartFindService" << std::endl;
    HandleNotificationData handle_notification_data{};
    auto find_service_callback = [&check_point_control, &handle_notification_data](auto service_handle_container,
                                                                                   auto find_service_handle) noexcept {
        std::cerr << "Reconnected Consumer Step (RC.1): find service handler called" << std::endl;
        if (service_handle_container.size() != 1)
        {
            std::cerr << "Reconnected Consumer Step (RC.1): Error - StartFindService() is expected to find 1 service "
                         "instance but found: "
                      << service_handle_container.size() << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
        std::lock_guard lock{handle_notification_data.mutex};
        handle_notification_data.handle = std::make_unique<TestServiceProxy::HandleType>(service_handle_container[0]);
        handle_notification_data.condition_variable.notify_all();
        std::cerr << "Reconnected Consumer Step (RC.1): FindServiceHandler handler done - found one service instance."
                  << std::endl;

        if (auto result = TestServiceProxy::StopFindService(find_service_handle); !result.has_value())
        {
            std::cout << "Consumer Step (RC.1): Error Occurred during StopFindService." << std::endl;
            std::cout << result.error() << std::endl;
            check_point_control.ErrorOccurred();
        }
        else
        {
            std::cout << "Consumer Step (RC.1): StopFindService was called." << std::endl;
        }
    };

    auto find_service_handle_result = StartFindService<TestServiceProxy>(
        "Reconnected Consumer Step (RC.1)", find_service_callback, kProxyInstanceSpecifier, check_point_control);
    if (!find_service_handle_result.has_value())
    {
        return;
    }

    // ********************************************************************************
    // Step (RC.2) - Wait for FindServiceHandler to be called. Call StopFindService in handler
    // ********************************************************************************
    std::cerr << "Reconnected Consumer Step (RC.2): Wait for FindServiceHandler to be called" << std::endl;
    std::unique_lock lock{handle_notification_data.mutex};
    const auto wait_result = handle_notification_data.condition_variable.wait_for(
        lock, kMaxHandleNotificationWaitTime, [&handle_notification_data] {
            return handle_notification_data.handle != nullptr;
        });
    lock.unlock();
    if (!wait_result)
    {
        std::cerr << "Reconnected tConsumer: Did not receive handle in time!" << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    check_point_control.CheckPointReached(1);
    std::cerr << "Reconnected Consumer: Finishing actions!" << std::endl;
}
}  // namespace score::mw::com::test
