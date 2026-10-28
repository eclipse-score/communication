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

#include <gtest/gtest.h>

#include <packetrouter/ApplicationRouter.h>
#include <packetrouter/PacketRouterProxy.h>
#include <runtime/ApplicationManager.h>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

class ProxyApplicationRouterFake final : public ApplicationRouter {
public:
    void init() override {}
    void start() override {}
    void stop() override {}
    void join() override {}
    void send_message(std::shared_ptr<MessageSD>) override {}
    void send_message(std::shared_ptr<MessageSOMEIP>) override {}
    void send_message(MessageSOMEIP&) override {}
#if defined(ENABLE_SOMEIP_IPC)
    void add_ipc_route(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, bool) override {}
    void send_ipc_message(std::shared_ptr<MessageSOMEIP>) override {}
    void send_ipc_message(MessageSOMEIP&, bool) override {}
    void send_ipc_response_message(MessageSOMEIP&, std::uint16_t, std::uint16_t) override {}
    void on_internal_request(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t) override {}
#endif // ENABLE_SOMEIP_IPC
};

TEST(PacketRouterProxy, handles_malformed_and_unhandled_messages_without_daemon) {
    auto application_router = std::make_shared<ProxyApplicationRouterFake>();
    ApplicationManager application("proxy-test", "", application_router);
    PacketRouterProxy proxy(&application);
    auto endpoint = std::make_shared<EndpointBase>();

    std::uint8_t invalid[] = {0x00};
    proxy.on_message(endpoint, invalid, sizeof(invalid));

    auto request = MessageBuilder::create_request_message(0x1001, 0x0003, 0x01);
    request->set_instance_id(0x0002);
    request->set_request_id(0x00090001);
    std::uint8_t buffer[128] = {};
    std::uint32_t length = 0;
    MessageBuilder::build_byte_stream(buffer, &length, *request);
    proxy.on_message(endpoint, buffer, length);

    proxy.send_message(request);
}

TEST(PacketRouterProxy, reconnect_and_lifecycle_are_safe_without_daemon) {
    auto application_router = std::make_shared<ProxyApplicationRouterFake>();
    ApplicationManager application("proxy-test", "", application_router);
    PacketRouterProxy proxy(&application);

    proxy.do_connect();
    proxy.on_reconnect();
    proxy.start();
    proxy.stop();
    proxy.join();
}
