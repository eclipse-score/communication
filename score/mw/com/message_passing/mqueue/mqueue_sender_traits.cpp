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
#include "score/mw/com/message_passing/mqueue/mqueue_sender_traits.h"

#include "score/language/safecpp/string_view/null_termination_check.h"

namespace score::mw::com::message_passing
{

score::cpp::expected<MqueueSenderTraits::file_descriptor_type, score::os::Error> MqueueSenderTraits::try_open(
    const std::string_view identifier,
    const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    // NOTE: Below use of `safecpp::GetPtrToNullTerminatedUnderlyingBufferOf()` will emit a deprecation warning here
    //       since it is used in conjunction with `std::string_view`. Thus, it must get fixed appropriately instead!
    //       For examples about how to achieve that, see
    //       broken_link_g/swh/safe-posix-platform/blob/master/score/language/safecpp/string_view/README.md
    return os_resources.mqueue->mq_open(
        safecpp::GetPtrToNullTerminatedUnderlyingBufferOf(identifier),
        score::os::Mqueue::OpenFlag::kWriteOnly | score::os::Mqueue::OpenFlag::kNonBlocking);
}

void MqueueSenderTraits::close_sender(const MqueueSenderTraits::file_descriptor_type file_descriptor,
                                      const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    score::cpp::ignore = os_resources.mqueue->mq_close(file_descriptor);
}

bool MqueueSenderTraits::IsOsResourcesValid(const FileDescriptorResourcesType& os_resources) noexcept
{
    return os_resources.mqueue != nullptr;
}

MqueueSenderTraits::OsResources MqueueSenderTraits::GetDefaultOSResources(
    score::cpp::pmr::memory_resource* memory_resource) noexcept
{
    return {score::os::Mqueue::Default(memory_resource)};
}

}  // namespace score::mw::com::message_passing
