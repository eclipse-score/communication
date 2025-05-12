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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_CONFIG_H
#define SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_CONFIG_H

#include <chrono>
#include <cstdint>
#include <optional>

namespace score::mw::com::message_passing
{

/// \brief Configuration parameters for Receiver
/// \details Separated into its own data type due to code complexity requirements
struct ReceiverConfig
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.

    /// \brief specifies the maximum number of messages kept in the receiver queue
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::int32_t max_number_message_in_queue = 10;
    /// \brief artificially throttles the receiver message loop to limit the processing rate of incoming messages
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<std::chrono::milliseconds> message_loop_delay = std::nullopt;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_RECEIVER_CONFIG_H
