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

namespace score::mw::com::message_passing
{

score::cpp::expected<MqueueSenderTraits::file_descriptor_type, score::os::Error> MqueueSenderTraits::try_open(
    const std::string_view identifier,
    const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    return os_resources.mqueue->mq_open(
        identifier.data(), score::os::Mqueue::OpenFlag::kWriteOnly | score::os::Mqueue::OpenFlag::kNonBlocking);
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
