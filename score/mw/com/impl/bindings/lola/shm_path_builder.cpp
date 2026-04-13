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
#include "score/mw/com/impl/configuration/global_configuration.h"
#include <sched.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace score::mw::com::impl::lola
{

namespace
{

constexpr auto kDataChannelPrefix = "lola-data-";
constexpr auto kControlChannelPrefix = "lola-ctl-";
constexpr auto kMethodChannelPrefix = "lola-methods-";
constexpr auto kAsilBControlChannelSuffix = "-b";

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

/// Emit file name of the method file to an ostream
///
/// \param out output ostream to use
void EmitMethodFileName(std::ostream& out,
                        const std::uint16_t service_id,
                        const LolaServiceInstanceId::InstanceId instance_id,
                        const ProxyInstanceIdentifier& proxy_instance_identifier) noexcept
{
    out << kMethodChannelPrefix;
    AppendServiceAndInstance(out, service_id, instance_id);

    out << '-';
    out << std::setfill('0') << std::setw(5) << proxy_instance_identifier.application_id << '-';
    out << std::setfill('0') << std::setw(5) << proxy_instance_identifier.proxy_instance_counter;
}

}  // namespace

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

std::string ShmPathBuilder::GetMethodChannelShmName(
    const LolaServiceInstanceId::InstanceId instance_id,
    const ProxyInstanceIdentifier& proxy_instance_identifier) const noexcept
{
    return EmitWithPrefix('/', [this, instance_id, proxy_instance_identifier](auto& out) noexcept {
        EmitMethodFileName(out, service_id_, instance_id, proxy_instance_identifier);
    });
}

}  // namespace score::mw::com::impl::lola
