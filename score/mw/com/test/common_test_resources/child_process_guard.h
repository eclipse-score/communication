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


#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_CHILD_PROCESS_GUARD_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_CHILD_PROCESS_GUARD_H

#include <amp_optional.hpp>

#include <sys/types.h>

namespace bmw::mw::com::test
{

/// \brief Wrapper around a process ID which provides helper functions for checking if it's already dead and killing it.
class ChildProcessGuard
{
  public:
    explicit ChildProcessGuard(pid_t pid) noexcept : pid_{pid} {}

    /// \brief Kills the child process, if it is not already dead.
    /// \return true in case the child could be successfully killed or was dead already, false else.
    bool KillChildProcess() noexcept;
    amp::optional<bool> IsProcessDead(bool should_block) noexcept;

    pid_t GetPid() const noexcept { return pid_.value(); }

  private:
    // pid_ will be filled on construction of the ChildProcessGuard and will be cleared after calling ChildProcessGuard.
    amp::optional<pid_t> pid_;
};

}  // namespace bmw::mw::com::test

#endif  // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_CHILD_PROCESS_GUARD_H
