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
#ifndef SCORE_LIB_MESSAGE_PASSING_MOCK_SERVER_FACTORY_MOCK_H
#define SCORE_LIB_MESSAGE_PASSING_MOCK_SERVER_FACTORY_MOCK_H

#include "score/message_passing/i_server_factory.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class ServerFactoryMock : public IServerFactory
{
  public:
    MOCK_METHOD(score::cpp::pmr::unique_ptr<IServer>,
                Create,
                (const ServiceProtocolConfig&, const ServerConfig&),
                (noexcept, override));

    virtual ~ServerFactoryMock() = default;  // virtual to make compiler happy
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_MOCK_SERVER_FACTORY_MOCK_H
