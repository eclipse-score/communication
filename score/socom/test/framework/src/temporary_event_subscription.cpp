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

#include "score/socom/temporary_event_subscription.hpp"

#include "score/socom/utilities.hpp"

using ::testing::_;
using ::testing::Assign;

namespace score::socom {

Temporary_event_subscription::Temporary_event_subscription(
    Client_connector& cc, Server_connector_callbacks_mock& sc_callbacks, Event_id const& event_id,
    Brokenness const& brokenness)
    : m_event_id{event_id}, m_cc{cc} {
    std::atomic<bool> callback_called{true};
    if (brokenness != Brokenness::no_server_reponse_second_requester) {
        callback_called = false;
        EXPECT_CALL(sc_callbacks, on_event_update_request(_, event_id))
            .WillOnce(Assign(&callback_called, true));
    }

    this->m_cc.subscribe_event(event_id, Event_mode::update_and_initial_value);
    wait_for_atomics(callback_called);
}

Temporary_event_subscription::Temporary_event_subscription(Client_connector& cc,
                                                           Event_id const& event_id)
    : m_event_id{event_id}, m_cc{cc} {
    this->m_cc.subscribe_event(event_id, Event_mode::update);
}

Temporary_event_subscription::~Temporary_event_subscription() {
    m_cc.unsubscribe_event(m_event_id);
}

}  // namespace score::socom
