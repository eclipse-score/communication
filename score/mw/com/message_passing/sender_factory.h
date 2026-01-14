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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_SENDER_FACTORY_H
#define SCORE_MW_COM_MESSAGE_PASSING_SENDER_FACTORY_H

#include "score/mw/com/message_passing/i_sender.h"
#include "score/mw/com/message_passing/sender_config.h"

#include <score/memory.hpp>
#include <score/stop_token.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

namespace score::mw::com::message_passing
{

/// \brief Factory, which creates instances of ISender.
/// \details Factory pattern serves two purposes here: Testability/mockability of senders and alternative
///          implementations of ISender. We initially have a POSIX MQ based implementation of ISender, but specific
///          implementations e.g. for QNX based on specific IPC mechanisms are expected.
class SenderFactory final
{
  public:
    /// \brief Creates an implementation instance of ISender.
    /// \details This is the factory create method for ISender instances. There a specific implementation of the
    ///          platform or a mock instance (see InjectSenderMock()) is returned.
    /// \todo Eventually extend the signature with some configuration parameter, from which factory can deduce, which
    ///       ISender impl. to create. Currently we only have POSIX MQ based ISender impl..
    /// \param identifier some identifier for the sender being created. Depending on the chosen impl. this might be
    ///        used or not.
    /// \param token stop_token to notify a stop request, in case the sender implementation does some long-running/
    ///        async activity.
    /// \param sender_config additional sender configuration parameters
    /// \param logging_callback output method for error messages since we cannot use regular logging (default: cerr)
    /// \param memory_resource memory resource for allocating the Sender object
    /// \return a platform specific implementation of a ISender or a mock.
    static score::cpp::pmr::unique_ptr<ISender> Create(
        const std::string_view identifier,
        const score::cpp::stop_token& token,
        const SenderConfig& sender_config = {},
        LoggingCallback logging_callback = &DefaultLoggingCallback,
        score::cpp::pmr::memory_resource* const memory_resource = score::cpp::pmr::get_default_resource());

    /// \brief Inject pointer to a mock instance, which shall be returned by all Create() calls.
    /// \param mock
    /// \param score::cpp::callback - to be set and further called before creation of SenderMockWrapper.
    // Suppress "AUTOSAR C++14 M3-2-2" and "AUTOSAR C++14 M3-2-4", The rule states: "The One Definition Rule shall not
    // be violated." and "An identifier with external linkage shall have exactly one definition.", respectively.
    // Symbol_FUN is defined in every lambda instantiation, which violates the ODR.
    // coverity[autosar_cpp14_m7_3_1_violation] false-positive: class method (Ticket-234468)
    static void InjectSenderMock(
        ISender* const mock,
        // coverity[autosar_cpp14_m3_2_2_violation]
        // coverity[autosar_cpp14_m3_2_4_violation]
        score::cpp::callback<void(const score::cpp::stop_token&)>&& = [](const score::cpp::stop_token&) noexcept {});

  private:
    static ISender* sender_mock_;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_SENDER_FACTORY_H
