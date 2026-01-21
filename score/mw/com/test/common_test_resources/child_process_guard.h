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

#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_CHILD_PROCESS_GUARD_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_CHILD_PROCESS_GUARD_H

#include <score/optional.hpp>

#include <sys/types.h>

namespace score::mw::com::test
{

/// \brief Wrapper around a process ID which provides helper functions for checking if it's already dead and killing it.
class ChildProcessGuard
{
  public:
    explicit ChildProcessGuard(pid_t pid) noexcept : pid_{pid} {}

    /// \brief Kills the child process, if it is not already dead.
    /// \return true in case the child could be successfully killed or was dead already, false else.
    bool KillChildProcess() noexcept;
    score::cpp::optional<bool> IsProcessDead(bool should_block) noexcept;

    pid_t GetPid() const noexcept
    {
        return pid_.value();
    }

  private:
    // pid_ will be filled on construction of the ChildProcessGuard and will be cleared after calling ChildProcessGuard.
    score::cpp::optional<pid_t> pid_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_CHILD_PROCESS_GUARD_H
