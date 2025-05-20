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

#ifndef PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_CONSUMER_HANDLE_NOTIFICATION_DATA_H
#define PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_CONSUMER_HANDLE_NOTIFICATION_DATA_H
#include "score/mw/com/test/partial_restart/test_datatype.h"

#include <condition_variable>
#include <memory>
#include <mutex>

namespace bmw::mw::com::test
{

    struct HandleNotificationData
    {
        std::mutex mutex{};
        std::condition_variable condition_variable{};
        std::unique_ptr<bmw::mw::com::test::TestServiceProxy::HandleType> handle{nullptr};
    };

} // namespace bmw::mw::com::test

#endif // PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_CONSUMER_HANDLE_NOTIFICATION_DATA_H
