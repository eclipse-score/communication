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

#include <endpoint/EndpointTCP.h>

using namespace lgsomeip;

struct EndpointTCPTestHost {
    void on_message(std::shared_ptr<Endpoint>, std::uint8_t*, std::size_t) {}
    void on_disconnect(std::shared_ptr<Endpoint>) {}
    void on_connect(std::shared_ptr<Endpoint>, std::shared_ptr<Endpoint>) {}
};

TEST(EndpointTCPClient, retry_policy_discards_only_after_retry_limit_or_error) {
    EndpointTCPClient<EndpointTCPTestHost> endpoint(nullptr);

    EXPECT_FALSE(endpoint.is_packet_need_discard(0, kMaxNumRetry));
    EXPECT_TRUE(endpoint.is_packet_need_discard(0, kMaxNumRetry + 1));
    EXPECT_TRUE(endpoint.is_packet_need_discard(-1, 0));
}

TEST(EndpointTCPClient, recognizes_server_and_client_magic_cookies) {
    EndpointTCPClient<EndpointTCPTestHost> endpoint(nullptr);
    std::uint8_t invalid_cookie[kLenMagicCookie] = {};

    EXPECT_TRUE(endpoint.is_magic_cookie(const_cast<std::uint8_t*>(kServerMagicCookie), kLenMagicCookie));
    EXPECT_TRUE(endpoint.is_magic_cookie(const_cast<std::uint8_t*>(kClientMagicCookie), kLenMagicCookie));
    EXPECT_FALSE(endpoint.is_magic_cookie(invalid_cookie, sizeof(invalid_cookie)));
}

TEST(EndpointTCPClient, stores_server_endpoint_reference) {
    EndpointTCPClient<EndpointTCPTestHost> client(nullptr);
    auto server = std::make_shared<EndpointTCPServer<EndpointTCPTestHost>>(nullptr);

    client.set_server_endpoint(server);

    EXPECT_EQ(client.get_server_endpoint(), server);
}

TEST(EndpointTCPServer, manages_client_file_descriptors) {
    EndpointTCPServer<EndpointTCPTestHost> server(nullptr);

    server.add_client_endpoint(10);
    server.add_client_endpoint(11);
    ASSERT_EQ(server.get_client_list(), (std::vector<std::int32_t>{10, 11}));

    server.remove_client_endpoint(10);
    EXPECT_EQ(server.get_client_list(), (std::vector<std::int32_t>{11}));
    server.remove_client_endpoint(99);
    EXPECT_EQ(server.get_client_list(), (std::vector<std::int32_t>{11}));
}
