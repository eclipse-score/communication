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


#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_TIMEOUT_SUPERVISOR_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_TIMEOUT_SUPERVISOR_H

#include <amp_callback.hpp>
#include <amp_optional.hpp>

#include <sys/types.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace bmw::mw::com::test
{

/// \brief Manages supervision of timeouts. I.e. you can start a timeout supervision of N milliseconds. If it times out
///        before the supervision is stopped again, a user provided callback will be called.
/// \details This helper class is needed as bmw::os::InterprocessNotification (more exact the
///          InterprocessConditionalVariable it encapsulates) doesn't support a wait with timeout!
class TimeoutSupervisor
{
  public:
    TimeoutSupervisor();
    ~TimeoutSupervisor();

    void StartSupervision(std::chrono::milliseconds timeout, amp::callback<void(void)> timeout_callback);
    void StopSupervision();

  private:
    void Supervision();

    std::mutex mutex_;
    std::condition_variable cond_var_;
    amp::optional<std::chrono::milliseconds> timeout_;
    bool shutdown_;
    amp::callback<void(void)> timeout_callback_{};
    std::thread supervision_thread_;
};

}  // namespace bmw::mw::com::test

#endif  // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_TIMEOUT_SUPERVISOR_H
