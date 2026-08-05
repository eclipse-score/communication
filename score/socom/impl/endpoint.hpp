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

#ifndef SCORE_SOCOM_ENDPOINT_HPP
#define SCORE_SOCOM_ENDPOINT_HPP

#include "score/socom/reference_token.hpp"

namespace score {
namespace socom {

template <typename T>
class Endpoint {
   public:
    explicit Endpoint(T& connector, Reference_token reference_token)
        : m_connector{&connector}, m_reference_token{std::move(reference_token)} {}

    template <typename MessageType>
    inline typename MessageType::Return_type send(MessageType message) const {
        return m_connector->receive(std::move(message));
    }

   private:
    T* m_connector;
    Reference_token m_reference_token;
};

namespace client_connector {
class Impl;
}  // namespace client_connector

namespace server_connector {
class Impl;
class Client_connection;
}  // namespace server_connector

using Server_connector_endpoint = Endpoint<::score::socom::server_connector::Client_connection>;
using Client_connector_endpoint = Endpoint<::score::socom::client_connector::Impl>;

using Server_connector_listen_endpoint = Endpoint<::score::socom::server_connector::Impl>;

}  // namespace socom
}  // namespace score

#endif  // SCORE_SOCOM_ENDPOINT_HPP
