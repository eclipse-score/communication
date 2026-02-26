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

#ifndef SCORE_MW_COM_TEST_PARTIAL_RESTART_CONSUMER_HANDLE_NOTIFICATION_DATA_H
#define SCORE_MW_COM_TEST_PARTIAL_RESTART_CONSUMER_HANDLE_NOTIFICATION_DATA_H
#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"
#include "score/mw/com/types.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>

namespace score::mw::com::test
{

struct HandleNotificationData
{
    // The handle variable alone is not enough to detect service disappearance.
    // When a callback is set up using the StartFindService function, it may be called twice - once with 0 handles and
    // once with multiple handles. If a thread is waiting and using the handle to
    // detect service disappearance. It will never become aware that the
    // service has disappeared, if the callbacks are executed first. To address this issue, an additional flag
    // for service disappearance is introduced.
    std::mutex mutex{};
    std::condition_variable condition_variable{};
    bool service_disappeared{false};  // used to detect spurious wakeup
    std::unique_ptr<score::mw::com::test::TestServiceProxy::HandleType> handle{nullptr};
};

void WaitTillServiceDisappears(HandleNotificationData& handle_notification_data);
bool WaitTillServiceAppears(HandleNotificationData& handle_notification_data,
                            const std::chrono::seconds max_handle_notification_time);

void HandleReceivedNotification(const ServiceHandleContainer<TestServiceProxy::HandleType> service_handle_container,
                                HandleNotificationData& handle_notification_data,
                                CheckPointControl& check_point_control);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PARTIAL_RESTART_CONSUMER_HANDLE_NOTIFICATION_DATA_H
