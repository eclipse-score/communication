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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_MQUEUE_RECEIVER_TRAITS_H
#define SCORE_MW_COM_MESSAGE_PASSING_MQUEUE_RECEIVER_TRAITS_H

#include "score/mw/com/message_passing/message.h"
#include "score/mw/com/message_passing/serializer.h"
#include "score/mw/com/message_passing/shared_properties.h"

#include "score/os/errno.h"
#include "score/os/mqueue.h"
#include "score/os/stat.h"
#include "score/os/unistd.h"

#include <score/assert.hpp>
#include <score/expected.hpp>
#include <score/vector.hpp>

#include <cstdint>
#include <string_view>

namespace score::mw::com::message_passing
{

class MqueueReceiverTraits
{
  public:
    // Suppress "AUTOSAR C++14 A0-1-1" rule finding. This rule states: "A project shall not contain instances of
    // non-volatile variables being given values that are not subsequently used"
    // False positive, variable is used.
    // coverity[autosar_cpp14_a0_1_1_violation]
    static constexpr std::size_t kConcurrency{1U};
    using file_descriptor_type = mqd_t;
    // coverity[autosar_cpp14_a0_1_1_violation]
    static constexpr file_descriptor_type INVALID_FILE_DESCRIPTOR{-1};

    struct OsResources
    {
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". We need these data elements to be organized into a coherent organized data structure.
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::Unistd> unistd{};
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::Mqueue> mqueue{};
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::Stat> os_stat{};
    };

    static OsResources GetDefaultOSResources(score::cpp::pmr::memory_resource* memory_resource) noexcept
    {
        return {score::os::Unistd::Default(memory_resource),
                score::os::Mqueue::Default(memory_resource),
                score::os::Stat::Default(memory_resource)};
    }

    using FileDescriptorResourcesType = OsResources;

    static score::cpp::expected<file_descriptor_type, score::os::Error> open_receiver(
        const std::string_view identifier,
        const score::cpp::pmr::vector<uid_t>& allowed_uids,
        const std::int32_t max_number_message_in_queue,
        const FileDescriptorResourcesType& os_resources) noexcept;

    static void close_receiver(const file_descriptor_type file_descriptor,
                               const std::string_view identifier,
                               const FileDescriptorResourcesType& os_resources) noexcept;

    static void stop_receive(const file_descriptor_type file_descriptor,
                             const FileDescriptorResourcesType& os_resources) noexcept;

    template <typename ShortMessageProcessor, typename MediumMessageProcessor>
    static score::cpp::expected<bool, score::os::Error> receive_next(const file_descriptor_type file_descriptor,
                                                            std::size_t thread,
                                                            ShortMessageProcessor fShort,
                                                            MediumMessageProcessor fMedium,
                                                            const FileDescriptorResourcesType& os_resources)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
        score::cpp::ignore = thread;

        std::uint32_t message_priority{0U};
        RawMessageBuffer buffer{};
        const auto received =
            os_resources.mqueue->mq_receive(file_descriptor, buffer.begin(), buffer.size(), &message_priority);
        if (received.has_value())
        {
            // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
            // a well-formed switch statement".
            // We don't need a break statement at the end of each case as we use return.
            // coverity[autosar_cpp14_m6_4_3_violation]
            switch (buffer.at(GetMessageTypePosition()))
            {
                // Suppress "AUTOSAR C++14 M6-4-5" rule finding. This rule declares: "An unconditional throw
                // or break statement shall terminate every non-empty switch-clause".
                // We don't need a break statement at the end of default case as we use return.
                // coverity[autosar_cpp14_m6_4_5_violation]
                case static_cast<std::underlying_type_t<MessageType>>(MessageType::kStopMessage):
                    return false;
                // coverity[autosar_cpp14_m6_4_5_violation]
                case static_cast<std::underlying_type_t<MessageType>>(MessageType::kShortMessage):
                {
                    const auto message = DeserializeToShortMessage(buffer);
                    fShort(message);
                    return true;
                }
                // coverity[autosar_cpp14_m6_4_5_violation]
                case static_cast<std::underlying_type_t<MessageType>>(MessageType::kMediumMessage):
                {
                    const auto message = DeserializeToMediumMessage(buffer);
                    fMedium(message);
                    return true;
                }
                // coverity[autosar_cpp14_m6_4_5_violation]
                default:
                    // ignore request from a misbehaving client
                    return true;
            }
        }
        else
        {
            return score::cpp::make_unexpected(received.error());
        }
    }

  private:
    static bool IsOsResourcesValid(const FileDescriptorResourcesType& os_resources) noexcept;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_MQUEUE_RECEIVER_TRAITS_H
