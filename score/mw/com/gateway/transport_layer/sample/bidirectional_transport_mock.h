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

#ifndef SCORE_MW_COM_GATEWAY_BIDIRECTIONAL_TRANSPORT_MOCK_H
#define SCORE_MW_COM_GATEWAY_BIDIRECTIONAL_TRANSPORT_MOCK_H

#include "score/mw/com/gateway/transport_layer/sample/i_bidirectional_transport.h"

#include <gmock/gmock.h>

namespace score::mw::com::gateway
{

class BidirectionalTransportMock : public IBidirectionalTransport
{
  public:
    MOCK_METHOD((score::ResultBlank), Setup, (), (override));
    MOCK_METHOD((void), Shutdown, (), (override));
    MOCK_METHOD((bool), IsConnected, (), (const, override));
    MOCK_METHOD((score::ResultBlank), SendRequest, (TransportMessage&), (override));
    MOCK_METHOD((score::ResultBlank), SendNotification, (TransportMessage&), (override));
    MOCK_METHOD((void), SetMessageHandler, (MessageHandler), (override));
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_BIDIRECTIONAL_TRANSPORT_MOCK_H
