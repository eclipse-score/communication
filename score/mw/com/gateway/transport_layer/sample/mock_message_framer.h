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

#ifndef SCORE_MW_COM_GATEWAY_MOCK_MESSAGE_FRAMER_H
#define SCORE_MW_COM_GATEWAY_MOCK_MESSAGE_FRAMER_H

#include "score/mw/com/gateway/transport_layer/sample/i_message_framer.h"

#include <gmock/gmock.h>

namespace score::mw::com::gateway
{

class MockMessageFramer : public IMessageFramer
{
  public:
    MOCK_METHOD((score::ResultBlank), SendMessage, (std::int32_t, const TransportMessage&), (override));
    MOCK_METHOD((std::unique_ptr<TransportMessage>), ReceiveMessage, (std::int32_t), (override));
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_MOCK_MESSAGE_FRAMER_H
