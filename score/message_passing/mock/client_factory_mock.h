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
#ifndef SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_FACTORY_MOCK_H
#define SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_FACTORY_MOCK_H

#include "score/message_passing/i_client_factory.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class ClientFactoryMock : public IClientFactory
{
  public:
    MOCK_METHOD(score::cpp::pmr::unique_ptr<IClientConnection>,
                Create,
                (const ServiceProtocolConfig&, const ClientConfig&),
                (noexcept, override));

    virtual ~ClientFactoryMock() = default;  // virtual to make compiler happy
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_FACTORY_MOCK_H
