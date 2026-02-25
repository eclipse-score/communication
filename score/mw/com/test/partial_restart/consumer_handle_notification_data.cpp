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

#include "score/mw/com/test/partial_restart/consumer_handle_notification_data.h"

#include <chrono>
#include <iostream>
#include <mutex>

namespace score::mw::com::test
{

void WaitTillServiceDisappears(HandleNotificationData& handle_notification_data)
{
    std::unique_lock lock{handle_notification_data.mutex};
    handle_notification_data.condition_variable.wait(lock, [&handle_notification_data] {
        return handle_notification_data.service_disappeared == true;
    });
    handle_notification_data.service_disappeared = false;
}

bool WaitTillServiceAppears(HandleNotificationData& handle_notification_data,
                            const std::chrono::seconds max_handle_notification_time)
{
    std::unique_lock lock{handle_notification_data.mutex};
    const auto wait_result = handle_notification_data.condition_variable.wait_for(
        lock, max_handle_notification_time, [&handle_notification_data] {
            return handle_notification_data.handle != nullptr;
        });
    return wait_result;
}

void HandleReceivedNotification(const ServiceHandleContainer<TestServiceProxy::HandleType> service_handle_container,
                                HandleNotificationData& handle_notification_data,
                                CheckPointControl& check_point_control)
{
    std::cout << "Consumer: find service handler called" << std::endl;
    if (service_handle_container.size() > 1)
    {
        std::cerr << "Consumer: Error - StartFindService() did find more than 1 service instance!" << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    else if (service_handle_container.size() == 1)
    {
        std::lock_guard lock{handle_notification_data.mutex};
        handle_notification_data.handle =
            std::make_unique<score::mw::com::test::TestServiceProxy::HandleType>(service_handle_container[0]);
        handle_notification_data.condition_variable.notify_one();
        std::cout << "Consumer: FindServiceHandler handler done - found one service instance." << std::endl;
    }
    else  // service container size == 0 -> initial empty find-result or service disappeared
    {
        std::cout << "Consumer: find service handler called with 0 instances." << std::endl;
        std::lock_guard lock{handle_notification_data.mutex};
        const auto handle_previously_existed = (handle_notification_data.handle != nullptr);
        handle_notification_data.handle.reset(nullptr);
        if (handle_previously_existed)
        {
            std::cout << "Consumer: FindServiceHandler handler done - service instance disappeared." << std::endl;
            handle_notification_data.service_disappeared = true;
            handle_notification_data.condition_variable.notify_one();
        }
    }
}

}  // namespace score::mw::com::test
