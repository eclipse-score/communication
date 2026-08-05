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

#ifndef SCORE_SOCOM_EVENT_HPP
#define SCORE_SOCOM_EVENT_HPP

#include <cstdint>

namespace score::socom {

/// \brief Alias for an event ID.
using Event_id = std::uint16_t;

/// \brief Mode of an event.
enum class Event_mode : std::uint8_t {
    update = 0U,              ///< Without initial value request.
    update_and_initial_value  ///< With initial value request.
};

/// \brief State of an event subscription.
enum class Event_state : std::uint8_t {
    /// Enabled_server_connector: There is no Client_connector subscribed to the event.
    /// Client_connector: The Enabled_server_connector did not acknowledge or reject the
    /// subscription.
    unsubscribed,
    /// Enabled_server_connector: There is at least one Client_connector subscribed to the event.
    /// Client_connector: The Enabled_server_connected acknowledged the subscription.
    subscribed
};

}  // namespace score::socom

#endif  // SCORE_SOCOM_EVENT_HPP
