/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_APPLICATION_ROUTER_H
#define LG_SOMEIP_APPLICATION_ROUTER_H

#include <memory>

#include <message/Message.h>

namespace lgsomeip {

class ApplicationRouter {
public:
    virtual ~ApplicationRouter() = default;

    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void join() = 0;

    virtual void send_message(std::shared_ptr<MessageSD> message) = 0;
    virtual void send_message(std::shared_ptr<MessageSOMEIP> message) = 0;
    virtual void send_message(MessageSOMEIP& message) = 0;

#if defined(ENABLE_SOMEIP_IPC)
    virtual void add_ipc_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_group_id,
                               std::uint16_t app_id, bool is_provider) = 0;
    virtual void send_ipc_message(std::shared_ptr<MessageSOMEIP> message) = 0;
    virtual void send_ipc_message(MessageSOMEIP& message, bool is_provider) = 0;
    virtual void send_ipc_response_message(MessageSOMEIP& message, std::uint16_t request_id, std::uint16_t app_id) = 0;
    virtual void on_internal_request(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t request_id,
                                     std::uint16_t app_id) = 0;
#endif // ENABLE_SOMEIP_IPC
};

} // namespace lgsomeip

#endif // LG_SOMEIP_APPLICATION_ROUTER_H
