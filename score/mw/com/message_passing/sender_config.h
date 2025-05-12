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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_SENDER_CONFIG_H
#define SCORE_MW_COM_MESSAGE_PASSING_SENDER_CONFIG_H

#include <chrono>
#include <cstdint>

namespace score::mw::com::message_passing
{

/// \brief Configuration parameters for Sender
/// \details Separated into its own data type due to code complexity requirements
struct SenderConfig
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.
    // In MISRA C++:2023 this is no longer an issue and it is covered by rule 14.1.1

    /// \brief specifies the maximum number of attempts to send a message (default: 5)
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::int32_t max_numbers_of_retry = 5;
    /// \brief specifies the delay before retrying to send a message (default: 0 ms)
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::chrono::milliseconds send_retry_delay = {};
    /// \brief specifies the delay before retrying to connect to the receiver (default: 5 ms)
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::chrono::milliseconds connect_retry_delay = std::chrono::milliseconds{5};
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_SENDER_CONFIG_H
