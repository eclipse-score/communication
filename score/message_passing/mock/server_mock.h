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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_NEW_SERVER_MOCK_H
#define SCORE_MW_COM_MESSAGE_PASSING_NEW_SERVER_MOCK_H

#include "score/message_passing/i_server.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class ServerMock : public IServer
{
  public:
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>,
                StartListening,
                (ConnectCallback, DisconnectCallback, MessageCallback, MessageCallback),
                (noexcept, override));
    MOCK_METHOD(void, StopListening, (), (noexcept, override));

    // mockable virtual destructor
    MOCK_METHOD(void, Destruct, (), (noexcept));
    ~ServerMock() override
    {
        Destruct();
    }
};

}  // namespace message_passing
}  // namespace score
#endif  // SCORE_MW_COM_MESSAGE_PASSING_NEW_SERVER_MOCK_H
