/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_I_BIDIRECTIONAL_TRANSPORT_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_I_BIDIRECTIONAL_TRANSPORT_H_

#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"
#include "score/result/result.h"

#include <score/callback.hpp>

#include <memory>

namespace score::mw::com::gateway
{

/// \brief Interface of BidirectionalTransport, introduced only for testing/mocking purposes
class IBidirectionalTransport
{
  public:
    using MessageHandler = score::cpp::callback<void(std::unique_ptr<TransportMessage>), 64>;

    virtual ~IBidirectionalTransport() = default;

    virtual score::ResultBlank Setup() = 0;
    virtual void Shutdown() = 0;

    virtual bool IsConnected() const = 0;

    virtual score::ResultBlank SendRequest(TransportMessage& message) = 0;
    virtual score::ResultBlank SendNotification(TransportMessage& message) = 0;

    virtual void SetMessageHandler(MessageHandler handler) = 0;
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_I_BIDIRECTIONAL_TRANSPORT_H_
