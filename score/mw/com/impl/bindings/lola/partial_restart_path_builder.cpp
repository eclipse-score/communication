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
#include "score/mw/com/impl/bindings/lola/partial_restart_path_builder.h"

#include "score/mw/com/impl/bindings/lola/path_builder.h"

namespace score::mw::com::impl::lola
{
namespace
{

constexpr auto kLoLaDir = "mw_com_lola/";
constexpr auto kPartialRestartDir = "partial_restart/";
// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Need to limit this functionality to QNX only.
// coverity[autosar_cpp14_a16_0_1_violation]
#if defined(__QNXNTO__)
constexpr auto kTmpPathPrefix = "/tmp_discovery/";
// coverity[autosar_cpp14_a16_0_1_violation]  Different implementation required for linux.
#else
constexpr auto kTmpPathPrefix = "/tmp/";
// coverity[autosar_cpp14_a16_0_1_violation]
#endif
constexpr auto kServiceUsageMarkerFileTag = "usage-";
constexpr auto kServiceExistenceMarkerFileTag = "existence-";

/// Emit file name of the service instance existence marker file to an ostream
///
/// \param out output ostream to use
void EmitServiceInstanceExistenceMarkerFileName(std::ostream& out,
                                                const std::uint16_t service_id,
                                                const LolaServiceInstanceId::InstanceId instance_id) noexcept
{
    out << kServiceExistenceMarkerFileTag;
    AppendServiceAndInstance(out, service_id, instance_id);
}

/// Emit file name of the service instance usage marker file to an ostream
///
/// \param out output ostream to use
void EmitServiceInstanceUsageMarkerFileName(std::ostream& out,
                                            const std::uint16_t service_id,
                                            const LolaServiceInstanceId::InstanceId instance_id) noexcept
{
    out << kServiceUsageMarkerFileTag;
    AppendServiceAndInstance(out, service_id, instance_id);
}

}  // namespace

std::string PartialRestartPathBuilder::GetServiceInstanceExistenceMarkerFilePath(
    const LolaServiceInstanceId::InstanceId instance_id) const noexcept
{
    return EmitWithPrefix(std::string{kTmpPathPrefix} + std::string{kLoLaDir} + std::string{kPartialRestartDir},
                          [this, instance_id](auto& out) {
                              EmitServiceInstanceExistenceMarkerFileName(out, service_id_, instance_id);
                          });
}

std::string PartialRestartPathBuilder::GetServiceInstanceUsageMarkerFilePath(
    const LolaServiceInstanceId::InstanceId instance_id) const noexcept
{
    return EmitWithPrefix(std::string{kTmpPathPrefix} + std::string{kLoLaDir} + std::string{kPartialRestartDir},
                          [this, instance_id](auto& out) {
                              EmitServiceInstanceUsageMarkerFileName(out, service_id_, instance_id);
                          });
}

std::string PartialRestartPathBuilder::GetLolaPartialRestartDirectoryPath() const noexcept
{
    return std::string{kTmpPathPrefix} + std::string{kLoLaDir} + std::string{kPartialRestartDir};
}

}  // namespace score::mw::com::impl::lola
