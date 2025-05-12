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
#include "score/mw/com/message_passing/qnx/resmgr_sender_traits.h"

#include "score/mw/com/message_passing/qnx/resmgr_traits_common.h"

namespace score
{
namespace mw
{
namespace com
{
namespace message_passing
{

using file_descriptor_type = ResmgrSenderTraits::file_descriptor_type;

score::cpp::expected<file_descriptor_type, score::os::Error> ResmgrSenderTraits::try_open(
    const std::string_view identifier,
    const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    const QnxResourcePath path{identifier};
    // This function in a banned list, however according to the requirement
    // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
    // NOLINTNEXTLINE(score-banned-function): See above
    return os_resources.fcntl->open(path.c_str(),
                                    score::os::Fcntl::Open::kWriteOnly | score::os::Fcntl::Open::kCloseOnExec);
}

void ResmgrSenderTraits::close_sender(const file_descriptor_type file_descriptor,
                                      const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    // This function in a banned list, however according to the requirement
    // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
    // NOLINTNEXTLINE(score-banned-function): See above
    score::cpp::ignore = os_resources.unistd->close(file_descriptor);
}

bool ResmgrSenderTraits::IsOsResourcesValid(const FileDescriptorResourcesType& os_resources) noexcept
{
    return (os_resources.unistd != nullptr) && (os_resources.fcntl != nullptr);
}

ResmgrSenderTraits::OsResources ResmgrSenderTraits::GetDefaultOSResources(
    score::cpp::pmr::memory_resource* memory_resource) noexcept
{
    return {score::os::Unistd::Default(memory_resource), score::os::Fcntl::Default(memory_resource)};
}

}  // namespace message_passing
}  // namespace com
}  // namespace mw
}  // namespace score
