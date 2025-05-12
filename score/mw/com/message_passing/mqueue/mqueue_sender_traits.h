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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_MQUEUE_SENDER_TRAITS_H
#define SCORE_MW_COM_MESSAGE_PASSING_MQUEUE_SENDER_TRAITS_H

#include "score/mw/com/message_passing/message.h"
#include "score/mw/com/message_passing/serializer.h"
#include "score/mw/com/message_passing/shared_properties.h"

#include "score/os/errno.h"
#include "score/os/mqueue.h"

#include <score/assert.hpp>
#include <score/expected.hpp>
#include <score/utility.hpp>

#include <cstdint>
#include <string_view>

namespace score::mw::com::message_passing
{

class MqueueSenderTraits
{
  public:
    using file_descriptor_type = mqd_t;
    // Suppress "AUTOSAR C++14 A0-1-1" rule finding. This rule states: "A project shall not contain instances of
    // non-volatile variables being given values that are not subsequently used"
    // False positive, variable is used.
    // coverity[autosar_cpp14_a0_1_1_violation]
    static constexpr file_descriptor_type INVALID_FILE_DESCRIPTOR{-1};

    struct OsResources
    {
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". We need these data elements to be organized into a coherent organized data structure.
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::Mqueue> mqueue{};
    };

    using FileDescriptorResourcesType = OsResources;

    static OsResources GetDefaultOSResources(score::cpp::pmr::memory_resource* memory_resource) noexcept;

    static score::cpp::expected<file_descriptor_type, score::os::Error> try_open(
        const std::string_view identifier,
        const FileDescriptorResourcesType& os_resources) noexcept;

    static void close_sender(const file_descriptor_type file_descriptor,
                             const FileDescriptorResourcesType& os_resources) noexcept;

    template <typename MessageFormat>
    static score::mw::com::message_passing::RawMessageBuffer prepare_payload(const MessageFormat& message) noexcept
    {
        return SerializeToRawMessage(message);
    }

    static score::cpp::expected_blank<score::os::Error> try_send(const file_descriptor_type file_descriptor,
                                                        const score::mw::com::message_passing::RawMessageBuffer& buffer,
                                                        const FileDescriptorResourcesType& os_resources) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
        return os_resources.mqueue->mq_send(
            file_descriptor, buffer.begin(), buffer.size(), score::mw::com::message_passing::GetMessagePriority());
    }

    /// \brief For POSIX mqueue, we assume strong non-blocking guarantee.
    ///
    /// \details We use mqueue with score::os::Mqueue::OpenFlag::kNonBlocking, therefore we assume strong non
    ///          blocking guarantee. The guarantee could only be violated by the OS, but in this case we are dealing
    ///          obviously with a ASIL-QM grade OS anyways, where the safety related notion of this API is already
    ///          broken!
    /// \return true
    static bool has_non_blocking_guarantee() noexcept
    {
        return true;
    }

  private:
    static bool IsOsResourcesValid(const FileDescriptorResourcesType& os_resources) noexcept;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_MQUEUE_SENDER_TRAITS_H
