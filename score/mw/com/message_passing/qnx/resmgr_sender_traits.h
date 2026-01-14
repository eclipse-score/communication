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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_RESMGR_SENDER_TRAITS_H
#define SCORE_MW_COM_MESSAGE_PASSING_RESMGR_SENDER_TRAITS_H

#include "score/os/fcntl.h"
#include "score/os/unistd.h"

#include <score/assert.hpp>
#include <score/expected.hpp>
#include <score/utility.hpp>

#include <cstdint>
#include <string_view>

namespace score::mw::com::message_passing
{

class ResmgrSenderTraits
{
  public:
    using file_descriptor_type = int32_t;
    // Suppress "AUTOSAR C++14 A0-1-1" rule findings. This rule states: "A project shall not contain instances of
    // non-volatile variables being given values that are not subsequently used." False positive, variable is used.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    static constexpr file_descriptor_type INVALID_FILE_DESCRIPTOR{-1};

    struct OsResources
    {
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". We need these data elements to be organized into a coherent organized data structure.
        // Suppress "AUTOSAR C++14 A9-6-1" rule findings. This rule states: "Data types used for interfacing with
        // hardware or conforming to communication protocols shall be trivial, standard-layout and only contain members
        // of types with defined sizes." False-positive: structure is not meant to be serialized
        // coverity[autosar_cpp14_a9_6_1_violation]
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::Unistd> unistd{};
        // coverity[autosar_cpp14_a9_6_1_violation]
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::os::Fcntl> fcntl{};
    };

    using FileDescriptorResourcesType = OsResources;

    static OsResources GetDefaultOSResources(score::cpp::pmr::memory_resource* memory_resource) noexcept;

    static score::cpp::expected<file_descriptor_type, score::os::Error> try_open(
        const std::string_view identifier,
        const FileDescriptorResourcesType& os_resources) noexcept;

    static void close_sender(const file_descriptor_type file_descriptor,
                             const FileDescriptorResourcesType& os_resources) noexcept;

    template <typename MessageFormat>
    static const MessageFormat& prepare_payload(const MessageFormat& message) noexcept
    {
        // Suppress "AUTOSAR C++14 A7-5-1", The rule states: "function shall not return a reference
        // or a pointer to a parameter that is passed by reference to const"
        // prepare_payload called with an lvalue parameter from an outer scope, function return refers to valid object.
        // coverity[autosar_cpp14_a7_5_1_violation]
        return message;
    }

    template <typename MessageFormat>
    static score::cpp::expected_blank<score::os::Error> try_send(const file_descriptor_type file_descriptor,
                                                        const MessageFormat& message,
                                                        const FileDescriptorResourcesType& os_resources) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(IsOsResourcesValid(os_resources), "OS resources are not valid!");
        // This function in a banned list, however according to the requirement
        // broken_link_c/issue/57467 it's allowed for IPC API that is mw::com (aka LoLa)
        // NOLINTNEXTLINE(score-banned-function): See above
        const auto result = os_resources.unistd->write(file_descriptor, &message, sizeof(message));
        if (!result.has_value())
        {
            return score::cpp::make_unexpected(result.error());
        }
        return {};
    }

    static constexpr bool has_non_blocking_guarantee() noexcept
    {
        return false;
    }

  private:
    static bool IsOsResourcesValid(const FileDescriptorResourcesType& os_resources) noexcept;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_RESMGR_SENDER_TRAITS_H
