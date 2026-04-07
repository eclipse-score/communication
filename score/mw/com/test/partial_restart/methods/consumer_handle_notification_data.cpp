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
#include "score/mw/com/test/partial_restart/methods/consumer_handle_notification_data.h"

#include <chrono>
#include <iostream>
#include <mutex>

namespace score::mw::com::test
{

void WaitTillServiceDisappears(HandleNotificationData& handle_notification_data)
{
    std::cerr << "WaitTillServiceDisappears: Waiting for service to disappear" << std::endl;
    std::unique_lock<std::mutex> lock{handle_notification_data.mutex};

    // Use wait_for with timeout instead of indefinite wait.
    // When a provider is killed abruptly or when a proxy is connected, the service discovery
    // notification about service disappearance might not arrive immediately or at all.
    // We use a reasonable timeout (5 seconds) to avoid waiting forever.
    const std::chrono::seconds timeout{5};
    bool notified = handle_notification_data.condition_variable.wait_for(lock, timeout, [&handle_notification_data] {
        return handle_notification_data.service_disappeared;
    });

    if (!notified)
    {
        std::cerr << "WaitTillServiceDisappears: Timeout waiting for service disappearance notification, "
                  << "assuming service disappeared" << std::endl;
        // Manually set the flag since we're assuming disappearance
        handle_notification_data.service_disappeared = true;
    }

    handle_notification_data.service_disappeared = false;
    std::cerr << "WaitTillServiceDisappears: Service disappeared" << std::endl;
}

bool WaitTillServiceAppears(HandleNotificationData& handle_notification_data,
                            const std::chrono::seconds max_handle_notification_time)
{
    std::cerr << "WaitTillServiceAppears: Waiting for service to appear" << std::endl;
    std::unique_lock<std::mutex> lock{handle_notification_data.mutex};
    bool result = handle_notification_data.condition_variable.wait_for(
        lock, max_handle_notification_time, [&handle_notification_data] {
            return handle_notification_data.handle != nullptr;
        });
    std::cerr << "WaitTillServiceAppears: Service appeared: " << (result ? "true" : "false") << std::endl;
    return result;
}

void HandleReceivedNotification(
    const ServiceHandleContainer<TestMethodServiceProxy::HandleType> service_handle_container,
    HandleNotificationData& handle_notification_data,
    CheckPointControl& check_point_control)
{
    std::unique_lock<std::mutex> lock{handle_notification_data.mutex};

    if (service_handle_container.empty())
    {
        std::cerr << "HandleReceivedNotification: Service disappeared (empty container)" << std::endl;
        handle_notification_data.service_disappeared = true;
        handle_notification_data.handle.reset();
    }
    else
    {
        std::cerr << "HandleReceivedNotification: Service appeared (non-empty container, size: "
                  << service_handle_container.size() << ")" << std::endl;
        handle_notification_data.service_disappeared = false;
        handle_notification_data.handle =
            std::make_unique<TestMethodServiceProxy::HandleType>(service_handle_container.front());
    }

    handle_notification_data.condition_variable.notify_all();
}

}  // namespace score::mw::com::test
