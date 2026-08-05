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

#ifndef SCORE_SOCOM_SERVER_CONNECTOR_MOCK_HPP
#define SCORE_SOCOM_SERVER_CONNECTOR_MOCK_HPP

#include <gmock/gmock.h>

#include "score/socom/server_connector.hpp"

namespace score::socom {

class Server_connector_mock : public Disabled_server_connector, public Enabled_server_connector {
   public:
    MOCK_METHOD(Enabled_server_connector*, enable, (), (noexcept, override));
    MOCK_METHOD(Disabled_server_connector*, disable, (), (noexcept, override));
    MOCK_METHOD(Result<Blank>, update_event, (Event_id, Payload), (noexcept, override));
    MOCK_METHOD(Result<Blank>, update_requested_event, (Event_id, Payload), (noexcept, override));
    MOCK_METHOD(Result<Event_mode>, get_event_mode, (Event_id), (const, noexcept, override));

    MOCK_METHOD(Result<Writable_payload>, allocate_event_payload, (Event_id event_id),
                (noexcept, override));

    MOCK_METHOD(Server_service_interface_definition const&, get_configuration, (),
                (const, noexcept, override));
    MOCK_METHOD(Service_instance const&, get_service_instance, (), (const, noexcept, override));
};

}  // namespace score::socom

#endif  // SCORE_SOCOM_SERVER_CONNECTOR_MOCK_HPP
