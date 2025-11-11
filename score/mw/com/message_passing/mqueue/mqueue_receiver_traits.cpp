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
#include "score/mw/com/message_passing/mqueue/mqueue_receiver_traits.h"

#include "score/language/safecpp/string_view/null_termination_check.h"

#include "score/os/unistd.h"

#include <score/utility.hpp>

namespace score::mw::com::message_passing
{

// Only one thread, SWS_CM_00182 (SCR-5898338) implicitly fulfilled for Mqueue implementation
constexpr std::size_t MqueueReceiverTraits::kConcurrency;

score::cpp::expected<MqueueReceiverTraits::file_descriptor_type, score::os::Error> MqueueReceiverTraits::open_receiver(
    const std::string_view identifier,
    const score::cpp::pmr::vector<uid_t>& allowed_uids,
    const std::int32_t max_number_message_in_queue,
    const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    score::cpp::ignore = allowed_uids;
    /* parameter only used in linux testing */
    using OpenFlag = score::os::Mqueue::OpenFlag;
    using Perms = score::os::Mqueue::ModeFlag;
    constexpr auto flags = OpenFlag::kCreate | OpenFlag::kReadWrite | OpenFlag::kCloseOnExec;
    // We allow access by all processes in the system since mqueues don't support setting ACLs under Linux.
    constexpr auto perms = Perms::kReadUser | Perms::kWriteUser | Perms::kWriteGroup | Perms::kWriteOthers;
    mq_attr queue_attributes{};
    queue_attributes.mq_msgsize = static_cast<long>(GetMaxMessageSize());
    queue_attributes.mq_maxmsg = static_cast<long>(max_number_message_in_queue);

    const auto& os_stat = os_resources.os_stat;
    const auto& mq = os_resources.mqueue;
    // Temporarily set the umask to 0 to allow for world-accessible queues.
    const auto old_umask = os_stat->umask(score::os::Stat::Mode::kNone).value();
    // NOTE: Below use of `safecpp::GetPtrToNullTerminatedUnderlyingBufferOf()` will emit a deprecation warning here
    //       since it is used in conjunction with `std::string_view`. Thus, it must get fixed appropriately instead!
    //       For examples about how to achieve that, see
    //       broken_link_g/swh/safe-posix-platform/blob/master/score/language/safecpp/string_view/README.md
    const auto result =
        mq->mq_open(safecpp::GetPtrToNullTerminatedUnderlyingBufferOf(identifier), flags, perms, &queue_attributes);
    // no error code is returned. And return value is ignored as it is not needed.
    score::cpp::ignore = os_stat->umask(old_umask).value();
    return result;
}

void MqueueReceiverTraits::close_receiver(const MqueueReceiverTraits::file_descriptor_type file_descriptor,
                                          const std::string_view identifier,
                                          const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    score::cpp::ignore = os_resources.mqueue->mq_close(file_descriptor);
    // NOTE: Below uses of `safecpp::GetPtrToNullTerminatedUnderlyingBufferOf()` will emit a deprecation warning here
    //       since it is used in conjunction with `std::string_view`. Thus, it must get fixed appropriately instead!
    //       For examples about how to achieve that, see
    //       broken_link_g/swh/safe-posix-platform/blob/master/score/language/safecpp/string_view/README.md
    score::cpp::ignore = os_resources.mqueue->mq_unlink(safecpp::GetPtrToNullTerminatedUnderlyingBufferOf(identifier));
    score::cpp::ignore = os_resources.unistd->unlink(safecpp::GetPtrToNullTerminatedUnderlyingBufferOf(identifier));
}

void MqueueReceiverTraits::stop_receive(const MqueueReceiverTraits::file_descriptor_type file_descriptor,
                                        const FileDescriptorResourcesType& os_resources) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
    constexpr auto message = static_cast<std::underlying_type<MessageType>::type>(MessageType::kStopMessage);
    score::cpp::ignore =
        os_resources.mqueue->mq_send(file_descriptor, &message, sizeof(MessageType), GetMessagePriority());
}

bool MqueueReceiverTraits::IsOsResourcesValid(const FileDescriptorResourcesType& os_resources) noexcept
{
    return ((os_resources.unistd != nullptr) && (os_resources.mqueue != nullptr)) && (os_resources.os_stat != nullptr);
}

}  // namespace score::mw::com::message_passing
