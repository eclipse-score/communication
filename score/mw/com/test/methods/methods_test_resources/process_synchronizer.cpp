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
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"

#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_guard.h"

#include "score/os/utils/interprocess/interprocess_notification.h"

#include <score/stop_token.hpp>

#include <unistd.h>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{

std::optional<ProcessSynchronizer> ProcessSynchronizer::Create(const std::string& interprocess_notification_shm_path)
{
    auto interprocess_notification_result =
        SharedMemoryObjectCreator<os::InterprocessNotification>::CreateOrOpenObject(interprocess_notification_shm_path);
    if (!interprocess_notification_result.has_value())
    {
        std::stringstream ss;
        ss << "Consumer: Creating or opening interprocess notification object failed:"
           << interprocess_notification_result.error().ToString();
        std::cerr << ss.str() << std::endl;
        return {};
    }

    return std::optional<ProcessSynchronizer>{std::in_place_t{}, std::move(interprocess_notification_result).value()};
}

ProcessSynchronizer::ProcessSynchronizer(
    SharedMemoryObjectCreator<os::InterprocessNotification> interprocess_notifier_creator)
    : interprocess_notifier_creator_{std::move(interprocess_notifier_creator)},
      interprocess_notifier_guard_{interprocess_notifier_creator_},
      interprocess_notifier_{interprocess_notifier_creator_.GetObject()}
{
}

void ProcessSynchronizer::Notify()
{
    interprocess_notifier_.notify();
}

bool ProcessSynchronizer::WaitWithAbort(score::cpp::stop_token stop_token)
{
    return interprocess_notifier_.waitWithAbort(stop_token);
}

}  // namespace score::mw::com::test
