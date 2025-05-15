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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_REGISTER_PID_FAKE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_REGISTER_PID_FAKE_H

#include "score/mw/com/impl/bindings/lola/uid_pid_mapping_entry.h"

#include "score/containers/dynamic_array.h"

#include <sys/types.h>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief Fake class that allows a test to inject the result to be returned by RegisterPid
///
/// RegisterPid is used by UidPidMapping, which resides in shared memory so we cannot rely on dynamic dispatch.
/// We would normally create an interface for a RegisterPidMock such that UidPidMapping depends only on the interface
/// and the test itself can add the dependency on the mock (which depends on gtest). Since we cannot do this, we want to
/// avoid that UidPidMapping depends on gtest in production so we instead use this simple fake class.
class RegisterPidFake
{
  public:
    std::optional<pid_t> RegisterPid(score::containers::DynamicArray<UidPidMappingEntry>::iterator,
                                     score::containers::DynamicArray<UidPidMappingEntry>::iterator,
                                     const uid_t,
                                     const pid_t) const
    {
        return expected_register_pid_result_;
    }

    void InjectRegisterPidResult(std::optional<pid_t> expected_register_pid_result)
    {
        expected_register_pid_result_ = expected_register_pid_result;
    }

  private:
    std::optional<pid_t> expected_register_pid_result_{};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_REGISTER_PID_FAKE_H
