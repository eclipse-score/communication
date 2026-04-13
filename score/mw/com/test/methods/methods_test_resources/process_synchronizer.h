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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROCESS_SYNCHRONIZER_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROCESS_SYNCHRONIZER_H

#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_guard.h"

#include "score/os/utils/interprocess/interprocess_notification.h"

#include <score/stop_token.hpp>

#include <cstdlib>

namespace score::mw::com::test
{

/// \brief InterprocessNotification wrapper class which creates an InterprocessNotification in shared memory and exposes
/// parts of the InterprocessNotification API
class ProcessSynchronizer
{
  public:
    static std::optional<ProcessSynchronizer> Create(const std::string& interprocess_notification_shm_path);

    ProcessSynchronizer(SharedMemoryObjectCreator<os::InterprocessNotification> interprocess_notifier_creator);

    void Notify();

    bool WaitWithAbort(score::cpp::stop_token stop_token);

  private:
    SharedMemoryObjectCreator<os::InterprocessNotification> interprocess_notifier_creator_;
    SharedMemoryObjectGuard<os::InterprocessNotification> interprocess_notifier_guard_;
    os::InterprocessNotification& interprocess_notifier_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROCESS_SYNCHRONIZER_H
