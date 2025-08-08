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
#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"

#include "score/mw/com/impl/bindings/lola/path_builder.h"

#include <iomanip>
#include <sstream>
#include <string>

namespace score::mw::com::impl::lola
{

namespace
{

constexpr auto kDataChannelPrefix = "lola-data-";
constexpr auto kControlChannelPrefix = "lola-ctl-";
constexpr auto kAsilBControlChannelSuffix = "-b";
// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Need to limit this functionality to QNX only.
// coverity[autosar_cpp14_a16_0_1_violation]
#if defined(__QNXNTO__)
constexpr auto kSharedMemoryPathPrefix = "/dev/shmem/";
// coverity[autosar_cpp14_a16_0_1_violation]  Different implementation required for linux.
#else
constexpr auto kSharedMemoryPathPrefix = "/dev/shm/";
// coverity[autosar_cpp14_a16_0_1_violation]
#endif

/// Emit file name of the control file to an ostream
///
/// \param out output ostream to use
/// \param channel_type ASIL level of the channel
/// \param instance_id InstanceId of path to be created
void EmitControlFileName(std::ostream& out,
                         const QualityType channel_type,
                         const std::uint16_t service_id,
                         const LolaServiceInstanceId::InstanceId instance_id) noexcept
{
    out << kControlChannelPrefix;
    AppendServiceAndInstance(out, service_id, instance_id);

    // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
    // a well-formed switch statement".
    // We don't need a break statement at the kInvalid case as we use fallthrough.
    // coverity[autosar_cpp14_m6_4_3_violation]
    switch (channel_type)
    {
        case QualityType::kASIL_QM:
        {
            break;
        }
        case QualityType::kASIL_B:
        {
            out << kAsilBControlChannelSuffix;
            break;
        }
        case QualityType::kInvalid:
            [[fallthrough]];
        default:
        {
            // Suppress "AUTOSAR C++14 M0-1-1", The rule states: "A project shall not contain unreachable code"
            // This is a false positive, this code is reachable due to fallthrough.
            // default case and break are necessary to have well-formed switch.
            // coverity[autosar_cpp14_m0_1_1_violation : FALSE]
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Invalid quality type");

            // Rationale: This code is actually unreachable due to the assert above which will always be triggered if
            // hit. However, rule M6-4-5 says that "An unconditional throw or break statement shall terminate every
            // nonempty switch-clause.". So to avoid triggering that warning, we suppress this warning about unreachable
            // code rather than removing the break statement.
            // coverity[autosar_cpp14_m0_1_1_violation]
            break;
        }
    }
}

/// Emit file name of the data file to an ostream
///
/// \param out output ostream to use
void EmitDataFileName(std::ostream& out,
                      const std::uint16_t service_id,
                      const LolaServiceInstanceId::InstanceId instance_id) noexcept
{
    out << kDataChannelPrefix;
    AppendServiceAndInstance(out, service_id, instance_id);
}

}  // namespace

std::string ShmPathBuilder::GetControlChannelFileName(const LolaServiceInstanceId::InstanceId instance_id,
                                                      const QualityType channel_type) const noexcept
{
    return EmitWithPrefix("", [this, channel_type, instance_id](auto& out) noexcept {
        EmitControlFileName(out, channel_type, service_id_, instance_id);
    });
}

std::string ShmPathBuilder::GetDataChannelFileName(const LolaServiceInstanceId::InstanceId instance_id) const noexcept
{
    return EmitWithPrefix("", [this, instance_id](auto& out) noexcept {
        EmitDataFileName(out, service_id_, instance_id);
    });
}

std::string ShmPathBuilder::GetControlChannelPath(const LolaServiceInstanceId::InstanceId instance_id,
                                                  const QualityType channel_type) const noexcept
{
    return EmitWithPrefix(kSharedMemoryPathPrefix, [this, channel_type, instance_id](auto& out) noexcept {
        EmitControlFileName(out, channel_type, service_id_, instance_id);
    });
}

std::string ShmPathBuilder::GetDataChannelPath(const LolaServiceInstanceId::InstanceId instance_id) const noexcept
{
    return EmitWithPrefix(kSharedMemoryPathPrefix, [this, instance_id](auto& out) noexcept {
        EmitDataFileName(out, service_id_, instance_id);
    });
}

std::string ShmPathBuilder::GetDataChannelShmName(const LolaServiceInstanceId::InstanceId instance_id) const noexcept
{
    return EmitWithPrefix('/', [this, instance_id](auto& out) {
        EmitDataFileName(out, service_id_, instance_id);
    });
}

std::string ShmPathBuilder::GetControlChannelShmName(const LolaServiceInstanceId::InstanceId instance_id,
                                                     const QualityType channel_type) const noexcept
{
    return EmitWithPrefix('/', [this, channel_type, instance_id](auto& out) noexcept {
        EmitControlFileName(out, channel_type, service_id_, instance_id);
    });
}

std::string ShmPathBuilder::GetPrefixContainingControlChannelAndServiceId(const std::uint16_t service_id) noexcept
{
    std::stringstream out{};
    out << kControlChannelPrefix;
    AppendService(out, service_id);
    return out.str();
}

std::string ShmPathBuilder::GetAsilBSuffix() noexcept
{
    return kAsilBControlChannelSuffix;
}

std::string ShmPathBuilder::GetSharedMemoryPrefix() noexcept
{
    return kSharedMemoryPathPrefix;
}

}  // namespace score::mw::com::impl::lola
