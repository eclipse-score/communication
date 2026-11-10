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

#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_GENERAL_RESOURCES_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_GENERAL_RESOURCES_H

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/child_process_guard.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"

#include "score/os/errno.h"

#include <score/assert.hpp>
#include <score/stop_token.hpp>
#include <optional>

#include <chrono>
#include <functional>
#include <string_view>
#include <vector>

namespace score::mw::com::test
{

/// \brief Helper class for cleaning up objects that must be destroyed when the test ends
class ObjectCleanupGuard
{
  public:
    void AddConsumerCheckpointControlGuard(
        SharedMemoryObjectCreator<CheckPointControl>& consumer_checkpoint_control_guard) noexcept;
    void AddProviderCheckpointControlGuard(
        SharedMemoryObjectCreator<CheckPointControl>& provider_checkpoint_control_guard) noexcept;
    void AddForkConsumerGuard(ChildProcessGuard& fork_consumer_pid_guard) noexcept;
    void AddForkProviderGuard(ChildProcessGuard& fork_provider_pid_guard) noexcept;
    bool CleanUp() noexcept;

  private:
    std::vector<std::reference_wrapper<SharedMemoryObjectCreator<CheckPointControl>>>
        consumer_checkpoint_control_guard_{};
    std::vector<std::reference_wrapper<SharedMemoryObjectCreator<CheckPointControl>>>
        provider_checkpoint_control_guard_{};
    std::vector<std::reference_wrapper<ChildProcessGuard>> fork_provider_pid_guard_{};
    std::vector<std::reference_wrapper<ChildProcessGuard>> fork_consumer_pid_guard_{};
};

void assertion_stdout_handler(const score::cpp::handler_parameters& param) noexcept;

/// \brief Helper function used in childs (consumer/provider) to receive and evaluate notifications from
///        parent/controller and to decide, whether the next checkpoint shall be reached or the consumer has to
///        finish/terminate.
///        Both: an explicit notification to terminate and an aborted (via stop-token) wait shall lead to
///        finish/terminate.
/// \param check_point_control CheckPointControl instance used in communication with controller.
/// \param test_stop_token test-wide/process-wide stop-token used for wait on notification.
/// \return returns ProceedInstruction indicating whether child should proceed to the next checkpoint or
/// finish/terminate.
CheckPointControl::ProceedInstruction WaitForChildProceed(CheckPointControl& check_point_control,
                                                          score::cpp::stop_token test_stop_token);

os::Result<SharedMemoryObjectCreator<CheckPointControl>> CreateSharedCheckPointControl(
    std::string_view message_prefix,
    std::string_view shared_memory_file_path,
    std::string_view check_point_owner_name) noexcept;

os::Result<SharedMemoryObjectCreator<CheckPointControl>> CreateOrOpenSharedCheckPointControl(
    std::string_view message_prefix,
    std::string_view shared_memory_file_path,
    std::string_view check_point_owner_name) noexcept;

os::Result<SharedMemoryObjectCreator<CheckPointControl>> OpenSharedCheckPointControl(
    std::string_view message_prefix,
    std::string_view shared_memory_file_path) noexcept;

std::optional<ChildProcessGuard> ForkProcessAndRunInChildProcess(std::string_view parent_message_prefix,
                                                                 std::string_view child_message_prefix,
                                                                 const std::function<void()>& child_callable) noexcept;

bool WaitForChildProcessToTerminate(std::string_view message_prefix,
                                    score::mw::com::test::ChildProcessGuard& child_process_guard,
                                    std::chrono::milliseconds max_wait_time) noexcept;

/// \brief Controller side: safely kills a child, which communicates with the controller via check_point_control.
///
/// The notifications in CheckPointControl are based on non-robust process-shared pthread mutexes. If a child is
/// SIGKILLed while it is blocked inside such a notification (e.g. in WaitForChildProceed()), it may still own an
/// internal mutex, which leaves the CheckPointControl unusable (deadlock on the next notify/wait). Therefore the
/// child is first told to leave the notification and to idle (see IdleUntilKilled()) and is only killed after it has
/// confirmed that.
/// \return true, if the child has been killed.
bool KillChildAfterItLeftNotification(std::string_view message_prefix,
                                      CheckPointControl& check_point_control,
                                      ChildProcessGuard& child_process_guard,
                                      std::chrono::milliseconds max_wait_time) noexcept;

/// \brief Child side counterpart of KillChildAfterItLeftNotification(): to be called after WaitForChildProceed()
///        returned ProceedInstruction::WAIT_FOR_KILL. Signals the controller and never returns.
[[noreturn]] void IdleUntilKilled(CheckPointControl& check_point_control) noexcept;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_GENERAL_RESOURCES_H
