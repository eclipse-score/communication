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

#ifndef SCORE_SOCOM_CLIENT_CONNECTOR_MOCK_HPP
#define SCORE_SOCOM_CLIENT_CONNECTOR_MOCK_HPP

#include <gmock/gmock.h>

#include "score/socom/client_connector.hpp"

namespace score::socom {

class Client_connector_mock : public Client_connector {
   public:
    MOCK_METHOD(Result<void>, subscribe_event, (Event_id client_id, Event_mode mode),
                (const, noexcept, override));
    MOCK_METHOD(Result<void>, unsubscribe_event, (Event_id), (const, noexcept, override));
    MOCK_METHOD(Result<void>, request_event_update, (Event_id), (const, noexcept, override));
    MOCK_METHOD(Result<Method_invocation::Uptr>, call_method,
                (Method_id, Payload, Method_call_reply_data_opt), (const, noexcept, override));
    MOCK_METHOD(Result<Writable_payload>, allocate_method_call_payload, (Method_id method_id),
                (noexcept, override));
    MOCK_METHOD(Result<Posix_credentials>, get_peer_credentials, (), (const, noexcept, override));
    MOCK_METHOD(Service_interface_definition const&, get_configuration, (),
                (const, noexcept, override));
    MOCK_METHOD(Service_instance const&, get_service_instance, (), (const, noexcept, override));
    MOCK_METHOD(bool, is_service_available, (), (const, noexcept, override));
};

}  // namespace score::socom

#endif  // SCORE_SOCOM_CLIENT_CONNECTOR_MOCK_HPP
