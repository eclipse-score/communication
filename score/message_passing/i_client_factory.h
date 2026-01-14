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
#ifndef SCORE_LIB_MESSAGE_PASSING_I_CLIENT_FACTORY_H
#define SCORE_LIB_MESSAGE_PASSING_I_CLIENT_FACTORY_H

#include "score/message_passing/i_client_connection.h"
#include "score/message_passing/service_protocol_config.h"

#include <score/memory.hpp>

#include <cstdint>

namespace score
{
namespace message_passing
{

/// \brief A generic factory interface to create instances of IClientConnection.
/// \details There may be multiple messaging client factories in the same process, implementing different transports
///          and/or having different configuration parameters for their client connections. An IClientFactory instance
///          reference should generally be provided as a parameter to the code that may create one or multiple
///          IClientConnection instances of the same kind.
///          The client factory instances shall have lifetimes longer than any active client connection in
///          IClientConnection instances produced by them, so, they are not supposed to be destroyed by the holders of
///          the pointers to the base class interface.
class IClientFactory
{
  public:
    // Suppress "AUTOSAR C++14 A9-6-1" rule findings. This rule declares: "Data types used for interfacing with hardware
    // or conforming to communication protocols shall be trivial, standard-layout and only contain members of types with
    // defined sizes."
    // False positive. These are configuration parameters.
    struct ClientConfig
    {
        std::uint32_t max_async_replies;  ///< Maximum number of SendWithCallback messages issued concurrently.
                                          ///< 0 if async replies are not used
        std::uint32_t max_queued_sends;   ///< Maximum number of Send messages queued on client side.
                                          ///< 0 if there is no client side queue
        // coverity[autosar_cpp14_a9_6_1_violation : FALSE]
        bool fully_ordered;  ///< true if the message delivery is serialized across delivery types
                             ///< (send-with-reply and fire-and-forget)
        // coverity[autosar_cpp14_a9_6_1_violation : FALSE]
        bool truly_async;  ///< true if Send and SendWithCallback calls always use background thread
                           ///< for IPC (requires nonzero max_queued_sends)
        // coverity[autosar_cpp14_a9_6_1_violation : FALSE]
        bool sync_first_connect;  ///< true if the first connection attempt uses the thread on which Start() is called
                                  ///< (can lead to deadlocks if the connection is established from within a callback)
    };

    /// \brief Creates an implementation instance of IClientConnection.
    /// \details This is the factory create method for IClientConnection instances. If the configuration parameters for
    ///          the messaging clients can be different, the instance of IClientFactory, depending on its implementation
    ///          and configuration, applies the uniform set of parameters associated with it or uses
    ///          _protocol_config.identifier_ to find a set of parameters associated with a particular client.
    ///          The function call itself is non-blocking, but the IClientConnection instance can be returned in its
    ///          Init state, not yet ready to send messages.
    /// \param protocol_config the service protocol data for the connection to create.
    /// \param client_config the client connection configuration.
    /// \return a platform specific implementation of an IClientConnection.
    virtual score::cpp::pmr::unique_ptr<IClientConnection> Create(const ServiceProtocolConfig& protocol_config,
                                                           const ClientConfig& client_config) noexcept = 0;

  protected:
    ~IClientFactory() = default;

    IClientFactory() noexcept = default;
    IClientFactory(const IClientFactory&) = delete;
    IClientFactory(IClientFactory&&) = delete;
    IClientFactory& operator=(const IClientFactory&) = delete;
    IClientFactory& operator=(IClientFactory&&) = delete;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_I_CLIENT_FACTORY_H
