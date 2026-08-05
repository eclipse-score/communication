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

#ifndef SCORE_SOCOM_RUNTIME_MOCK_HPP
#define SCORE_SOCOM_RUNTIME_MOCK_HPP

#include <gmock/gmock.h>

#include "score/socom/runtime.hpp"

namespace score::socom {

class Service_bridge_registration_handle_mock : public Service_bridge_registration_handle {
   public:
    // mock interface
    MOCK_METHOD(Bridge_identity, get_identity, (), (const, override));
};

class Runtime_mock : public Runtime {
   public:
    // mock interface
    MOCK_METHOD(Result<Client_connector::Uptr>, make_client_connector,
                (Service_interface_definition, Service_instance, Client_connector::Callbacks),
                (noexcept, override));
    MOCK_METHOD(Result<Client_connector::Uptr>, make_client_connector,
                (Service_interface_definition, Service_instance, Client_connector::Callbacks,
                 Posix_credentials const&),
                (noexcept, override));
    MOCK_METHOD((Result<Disabled_server_connector::Uptr>), make_server_connector,
                (Server_service_interface_definition, Service_instance,
                 Disabled_server_connector::Callbacks),
                (noexcept, override));
    MOCK_METHOD((Result<Disabled_server_connector::Uptr>), make_server_connector,
                (Server_service_interface_definition, Service_instance,
                 Disabled_server_connector::Callbacks, Posix_credentials const&),
                (noexcept, override));
    MOCK_METHOD(Result<Service_bridge_registration>, register_service_bridge,
                (Bridge_identity, Request_service_function), (noexcept, override));
};

}  // namespace score::socom

#endif  // SCORE_SOCOM_RUNTIME_MOCK_HPP
