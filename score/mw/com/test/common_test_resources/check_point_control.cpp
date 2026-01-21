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

#include "score/mw/com/test/common_test_resources/check_point_control.h"

#include "score/mw/com/test/common_test_resources/timeout_supervisor.h"

#include <score/stop_token.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{

CheckPointControl::CheckPointControl(const std::string_view check_point_owner_name) noexcept
    : owner_name_{check_point_owner_name}
{
}

/// \brief Function called by the Controller to trigger child process (Consumer or Producer) to proceed with its
///        actions until it reaches its next checkpoint (or an error)
void CheckPointControl::ProceedToNextCheckpoint() noexcept
{
    proceed_instruction_ = ProceedInstruction::PROCEED_NEXT_CHECKPOINT;
    proceed_notifier_.notify();
}

/// \brief Function called by the Controller to trigger child process (Consumer or Producer) to finish with its
///        actions and return/exit (with EXIT_SUCCESS)
void CheckPointControl::FinishActions() noexcept
{
    proceed_instruction_ = ProceedInstruction::FINISH_ACTIONS;
    proceed_notifier_.notify();
}

void CheckPointControl::WaitForKill() noexcept
{
    proceed_instruction_ = ProceedInstruction::WAIT_FOR_KILL;
    proceed_notifier_.notify();
}

/// \brief Function called by child process (Consumer or Producer) to notify its parent/controller, that it has
///        reached the given checkpoint.
/// \param checkpoint_number number of checkpoint, which has been reached.
void CheckPointControl::CheckPointReached(std::uint8_t checkpoint_number) noexcept
{
    checkpoint_reached_ = checkpoint_number;
    error_occurred_ = false;
    checkpoint_reached_notifier_.notify();
}

/// \brief Function called by child process (Consumer or Producer) to notify its parent/controller, that it has
///        encountered an error (hindering it to reach the next checkpoint)
void CheckPointControl::ErrorOccurred() noexcept
{
    checkpoint_reached_ = kInvalidCheckpointNumber;
    error_occurred_ = true;
    checkpoint_reached_notifier_.notify();
}

bool CheckPointControl::WaitForProceedOrFinishTrigger(score::cpp::stop_token stop_token) noexcept
{
    return proceed_notifier_.waitWithAbort(stop_token);
}

bool VerifyCheckpoint(const std::string& tag,
                      bool notification_happened,
                      CheckPointControl& checkpoint_control,
                      std::uint8_t expected_check_point)
{
    if (!notification_happened)
    {
        std::cerr << tag << checkpoint_control.GetOwnerName() << " failed: didn't reach checkpoint in time!"
                  << std::endl;
        return false;
    }
    // we received a notification. Reset notifier to enable further/new check-point-reached/error notifications.
    checkpoint_control.ResetCheckpointReachedNotifications();
    if (checkpoint_control.HasErrorOccurred())
    {
        std::cerr << tag << ": " << checkpoint_control.GetOwnerName() << " failed: reported Error." << std::endl;
        return false;
    }
    if (checkpoint_control.GetReachedCheckPoint() != expected_check_point)
    {
        std::cerr << tag << ": " << checkpoint_control.GetOwnerName() << " failed: reached unexpected checkpoint "
                  << static_cast<int>(checkpoint_control.GetReachedCheckPoint())
                  << ". Expected: " << static_cast<int>(expected_check_point) << std::endl;
        return false;
    }
    std::cout << tag << ": " << checkpoint_control.GetOwnerName() << " reached checkpoint "
              << static_cast<int>(expected_check_point) << std::endl;
    return true;
}

int WaitAndVerifyCheckPoint(const std::string& tag,
                            CheckPointControl& check_point_control,
                            std::uint8_t check_point,
                            score::cpp::stop_token token,
                            std::chrono::milliseconds wait_duration)
{
    TimeoutSupervisor timeout_supervisor{};
    std::cout << tag << "wait till check point " << static_cast<int>(check_point) << " is reached\n";
    auto notification_happened =
        check_point_control.WaitForCheckpointReachedOrError(wait_duration, token, timeout_supervisor);
    if (!VerifyCheckpoint(tag, notification_happened, check_point_control, check_point))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
