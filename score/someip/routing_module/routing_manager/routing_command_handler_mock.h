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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_ROUTING_MANAGER_ROUTING_COMMAND_HANDLER_MOCK_H
#define SCORE_SOMEIP_ROUTING_MODULE_ROUTING_MANAGER_ROUTING_COMMAND_HANDLER_MOCK_H

#include "score/someip/routing_module/routing_manager/i_routing_command_handler.h"

#include <gmock/gmock.h>

namespace score::someip::routing_module
{

class RoutingCommandHandlerMock : public IRoutingCommandHandler
{
  public:
    MOCK_METHOD(CommandResult, AddRemoteService, (const routing_control_protocol::Message&), (noexcept, override));
    MOCK_METHOD(CommandResult, RemoveRemoteService, (ServiceInstance), (noexcept, override));
    MOCK_METHOD(CommandResult, AddLocalSubscription, (ServiceInstance, std::uint16_t), (noexcept, override));
    MOCK_METHOD(CommandResult, RemoveLocalSubscription, (ServiceInstance, std::uint16_t), (noexcept, override));
    MOCK_METHOD(CommandResult, AddLocalService, (const routing_control_protocol::Message&), (noexcept, override));
    MOCK_METHOD(CommandResult, RemoveLocalService, (ServiceInstance), (noexcept, override));
    MOCK_METHOD(CommandResult, AddRemoteSubscription, (ServiceInstance, std::uint16_t, Endpoint), (noexcept, override));
    MOCK_METHOD(CommandResult,
                RemoveRemoteSubscription,
                (ServiceInstance, std::uint16_t, Endpoint),
                (noexcept, override));
    MOCK_METHOD(void, RemoveAll, (), (noexcept, override));
};

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_ROUTING_MANAGER_ROUTING_COMMAND_HANDLER_MOCK_H
