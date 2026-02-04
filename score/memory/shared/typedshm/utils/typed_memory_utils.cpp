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
#include "score/memory/shared/typedshm/utils/typed_memory_utils.h"
#include "score/os/errno_logging.h"
#include "score/os/stat.h"
#include "score/os/unistd.h"
#include "score/mw/log/logging.h"

#include <pwd.h>
#include <vector>

namespace score::memory::shared
{

namespace
{
// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Rationale: Pre-processor commands are used to allow different implementations for linux and QNX to exist in the same
// file. This keeps both implementations close (i.e. within the same functions) which makes the code easier to read and
// maintain. It also prevents compiler errors in linux code when compiling for QNX and vice versa.
// coverity[autosar_cpp14_a16_0_1_violation]
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
constexpr auto kTSHMDeviceName = "/dev/typedshm";
constexpr auto kTypedmemdUserName = "typed_memory_daemon";
constexpr std::uint32_t kMaxBufferSize = 16384U;
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif
}  // namespace

std::optional<uid_t> AcquireTypedMemoryDaemonUid() noexcept
{
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#if defined(__QNX__) && defined(USE_TYPEDSHMD)
    passwd pwd{};
    passwd* result = nullptr;
    std::vector<char> buffer(kMaxBufferSize);
    score::os::StatBuffer stat_buffer{};

    const auto stat_result = score::os::Stat::instance().stat(kTSHMDeviceName, stat_buffer);
    if (!stat_result.has_value())
    {
        // When the file /dev/typedshm doesn't exist, it means that the typed memory daemon isn't running.
        // This is expected in some scenarios like in SCTF, so we log error only in other cases.
        if (stat_result.error() != score::os::Error::Code::kNoSuchFileOrDirectory)
        {
            score::mw::log::LogError("shm") << __func__ << __LINE__ << "TypedMemoryDaemonUid cannot be acquired."
                                          << "Reason:" << stat_result.error();
        }
        return std::nullopt;
    }

    const auto pw =
        score::os::Unistd::instance().getpwnam_r(kTypedmemdUserName, &pwd, buffer.data(), buffer.size(), &result);
    if (pw.has_value())
    {
        if (result == nullptr)
        {
            score::mw::log::LogError("shm")
                << "AcquireTypedMemoryDaemonUid: user" << std::string{kTypedmemdUserName} << "not found";
            return std::nullopt;
        }
        score::mw::log::LogDebug("shm") << "AcquireTypedMemoryDaemonUid: typed_memory_daemon uid:" << pwd.pw_uid;
        return static_cast<uid_t>(pwd.pw_uid);
    }
    score::mw::log::LogError("shm") << "AcquireTypedMemoryDaemonUid failed:" << pw.error();
    return std::nullopt;
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#else
    return std::nullopt;
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif
}
}  // namespace score::memory::shared
