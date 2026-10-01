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

#include <cstdio>
#include <fstream>
#include <endpoint/EndpointBase.h>
#include <endpoint/EndpointUtils.h>
#include <runtime/ServiceManager.h>
#include <socket/IP4Address.h>
#include <socket/Socket.h>

#include <unistd.h>

#define private public
#include <packetrouter/PacketRouterHost.h>
#undef private

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

class PacketRouterServiceFake final : public ServiceRouter {
public:
    void init() override {}
    void start() override {}
    void stop() override {}
    std::shared_ptr<Multiplexer> get_multiplexer() override {
        return nullptr;
    }
    std::shared_ptr<Endpoint> get_service_discovery_multicast_endpoint() override {
        return nullptr;
    }
    std::shared_ptr<Endpoint> get_service_discovery_unicast_endpoint() override {
        return nullptr;
    }
    std::shared_ptr<Address> make_address() override {
        return std::make_shared<IP4Address>();
    }
    void set_instance_id(std::uint16_t, std::uint16_t, std::uint16_t) override {}
    bool add_route(std::uint16_t, std::uint16_t, std::uint16_t, std::shared_ptr<Address>, std::uint16_t,
                   std::shared_ptr<Address>, std::uint16_t, std::uint8_t,
                   std::shared_ptr<std::vector<std::shared_ptr<Address>>>) override {
        return true;
    }
    void remove_route(std::uint16_t, std::uint16_t) override {}
    std::int32_t find_connection(std::uint16_t, std::uint16_t, std::shared_ptr<Address>) override {
        return -1;
    }
    void add_subscribe_route(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, std::shared_ptr<Address>,
                             std::uint16_t) override {}
    void remove_subscribe_route(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t,
                                std::shared_ptr<Address>) override {}
    void remove_subscribe_route(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t) override {}
    void send_internal_message(std::uint8_t*, std::size_t, std::uint16_t) override {}
    void send_external_sd_message(std::uint8_t*, std::size_t, std::shared_ptr<Address>, bool) override {}
    void check_and_send_magic_cookies() override {}
    void reboot_route(std::shared_ptr<Address>) override {}
};

class CapturingEndpoint final : public EndpointBase {
public:
    void send_message(const std::uint8_t* message, std::size_t size, std::shared_ptr<Address> = nullptr) override {
        ++message_count;
        last_message.assign(message, message + size);
    }

    int message_count = 0;
    std::vector<std::uint8_t> last_message;
};

std::unique_ptr<ServiceManager> make_service_manager() {
    return std::make_unique<ServiceManager>("packet-router-test", "", std::make_shared<PacketRouterServiceFake>());
}

class ScopedPacketRouterConfig final {
public:
    ScopedPacketRouterConfig() : path_("/tmp/lgsomeip-packet-router-test-" + std::to_string(::getpid()) + ".json") {
        std::ofstream file(path_);
        file << R"({
            "unicast": "127.0.0.1",
            "applications": [],
            "services": [
                {
                    "service": "0x1001",
                    "instance": "0x0002",
                    "unreliable": "30509",
                    "is-provider": "false"
                }
            ],
            "service-discovery": {
                "enable": "true",
                "multicast": "224.244.224.245",
                "port": "30490",
                "protocol": "udp",
                "initial_delay_min": "10",
                "initial_delay_max": "100",
                "repetitions_base_delay": "200",
                "repetitions_max": "3",
                "ttl": "3",
                "cyclic_offer_delay": "2000",
                "request_response_delay": "1500"
            }
        })";
    }

    ~ScopedPacketRouterConfig() {
        std::remove(path_.c_str());
    }

    const std::string& path() const {
        return path_;
    }

private:
    std::string path_;
};

TEST(PacketRouterHost, find_instance_id_returns_zero_when_service_unmapped) {
    PacketRouterHost router(nullptr);
    auto endpoint = std::make_shared<EndpointBase>();
    endpoint->set_app_id(0x0005);

    EXPECT_EQ(router.find_instance_id(0x1001, endpoint), 0);
}

TEST(PacketRouterHost, find_instance_id_resolves_local_endpoint_mapped_by_app_id) {
    PacketRouterHost router(nullptr);
    auto endpoint = std::make_shared<EndpointBase>();
    endpoint->set_app_id(0x0005);

    router.set_instance_id(0x1001, 0x0002, static_cast<std::uint16_t>(0x0005));

    EXPECT_EQ(router.find_instance_id(0x1001, endpoint), 0x0002);
}

TEST(PacketRouterHost, find_instance_id_short_circuits_when_endpoint_already_has_instance_id) {
    PacketRouterHost router(nullptr);
    auto endpoint = std::make_shared<EndpointBase>();
    endpoint->set_app_id(0x0005);
    endpoint->set_instance_id(0x0009);

    // No mapping was registered for this app id; the endpoint's own instance id must win.
    EXPECT_EQ(router.find_instance_id(0x1001, endpoint), 0x0009);
}

TEST(PacketRouterHost, find_instance_id_resolves_udp_endpoint_mapped_by_address) {
    PacketRouterHost router(nullptr);
    auto address = std::make_shared<IP4Address>();
    address->set_reliable(false);
    address->set_port_address(30509);

    router.set_instance_id(0x1001, 0x0003, address);

    auto socket = std::make_shared<Socket>(address);
    auto endpoint = std::make_shared<EndpointBase>(socket);

    EXPECT_EQ(router.find_instance_id(0x1001, endpoint), 0x0003);
}

TEST(PacketRouterHost, creates_configured_ipv4_addresses) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());

    auto address = router.make_address(true, 30509, 3);

    ASSERT_NE(address, nullptr);
    EXPECT_EQ(address->get_type(), AF_INET);
    EXPECT_TRUE(address->get_reliable());
    EXPECT_EQ(address->get_port_address(), 30509);
    EXPECT_EQ(address->get_vlan_priority(), 3);
}

TEST(PacketRouterHost, manages_internal_route_and_subscription_without_sockets) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());

    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));
    router.add_subscribe_route(0x1001, 0x0002, 0x0003, 0x0005);
    router.remove_subscribe_route(0x1001, 0x0002, 0x0003, 0x0005);
    router.remove_subscribe_route(0x0005);
    router.remove_route(0x1001, 0x0002);

    auto address = std::make_shared<IP4Address>();
    address->set_reliable(false);
    address->set_port_address(30509);
    EXPECT_EQ(router.find_connection(0x1001, 0x0002, address), -1);
}

TEST(PacketRouterHost, find_connection_returns_zero_for_unicast_udp_route) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));

    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("192.0.2.10");
    address->set_reliable(false);
    address->set_port_address(30509);

    EXPECT_EQ(router.find_connection(0x1001, 0x0002, address), 0);
}

TEST(PacketRouterHost, gets_service_instance_for_internal_route) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto endpoint = std::make_shared<EndpointBase>();
    endpoint->set_app_id(0x0005);

    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));

    auto service = router.get_service_instance(0x1001, endpoint);

    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->app_id, 0x0005);
    EXPECT_EQ(endpoint->get_instance_id(), 0x0002);
}

TEST(PacketRouterHost, handles_unknown_route_cleanup_and_tcp_lookup) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto tcp_address = std::make_shared<IP4Address>();
    tcp_address->set_reliable(true);
    tcp_address->set_port_address(30509);

    router.remove_route(0x1001, 0x0002);
    EXPECT_EQ(router.find_connection(0x1001, 0x0002, tcp_address), -1);

    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));
    EXPECT_EQ(router.find_connection(0x1001, 0x0002, tcp_address), -1);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, detects_repeated_reboot_session) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());

    auto source = std::make_shared<IP4Address>();
    source->set_ip_address("192.0.2.20");
    source->set_reliable(false);
    source->set_port_address(40000);
    auto destination = std::make_shared<IP4Address>();
    destination->set_ip_address("192.0.2.21");
    destination->set_reliable(false);
    destination->set_port_address(30490);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(destination, source));
    endpoint->get_sender_address()->set_ip_address("192.0.2.20");

    EXPECT_FALSE(router.is_reboot(endpoint, true, 1));
    EXPECT_TRUE(router.is_reboot(endpoint, true, 1));
    EXPECT_FALSE(router.is_reboot(endpoint, false, 2));
}

TEST(PacketRouterHost, empty_send_and_cookie_operations_are_safe) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    std::uint8_t message[] = {0x00};

    router.send_internal_message(message, sizeof(message), 0x0005);
    router.send_external_sd_message(message, sizeof(message));
    router.check_and_send_magic_cookies();
}

TEST(PacketRouterHost, ignores_invalid_internal_message) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto endpoint = std::make_shared<EndpointBase>();
    std::uint8_t message[] = {0x00};

    router.on_internal_message(endpoint, message, sizeof(message));
}

TEST(PacketRouterHost, ignores_internal_callbacks_without_service_or_request_state) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto endpoint = std::make_shared<EndpointBase>();
    std::uint8_t message[32] = {};

    router.on_internal_request(endpoint, message, sizeof(message), 0x10010001, 0x00050001);
    router.on_internal_response(endpoint, message, sizeof(message), 0x10010001, 0x00050001);
    router.on_internal_notification(endpoint, message, sizeof(message), 0x10010002, 0x00050001);
}

TEST(PacketRouterHost, creates_and_removes_udp_provider_route) {
    ScopedPacketRouterConfig config_file;
    auto service_router = std::make_shared<PacketRouterServiceFake>();
    ServiceManager manager("packet-router-test", config_file.path(), service_router);
    PacketRouterHost router(&manager);
    auto multicast = std::make_shared<IP4Address>();
    multicast->set_ip_address("239.0.0.1");
    multicast->set_port_address(35682);
    multicast->set_reliable(false);
    auto multicast_list = std::make_shared<std::vector<std::shared_ptr<Address>>>();
    multicast_list->push_back(multicast);

    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005, nullptr, 35681, nullptr, 35679, 0xff, multicast_list));

    auto endpoint = std::make_shared<EndpointBase>();
    endpoint->set_app_id(0x0005);
    EXPECT_NE(router.get_service_instance(0x1001, endpoint), nullptr);

    router.remove_route(0x1001, 0x0002);
    EXPECT_EQ(router.find_connection(0x1001, 0x0002, std::make_shared<IP4Address>()), -1);
}

TEST(PacketRouterHost, creates_and_removes_udp_client_route) {
    ScopedPacketRouterConfig config_file;
    auto service_router = std::make_shared<PacketRouterServiceFake>();
    ServiceManager manager("packet-router-test", config_file.path(), service_router);
    PacketRouterHost router(&manager);

    auto remote = std::make_shared<IP4Address>();
    remote->set_ip_address("192.0.2.40");
    remote->set_port_address(40500);
    remote->set_reliable(false);

    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0, nullptr, 0, remote, 35680));
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, rejects_route_without_transport_endpoint) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());

    EXPECT_FALSE(router.add_route(0x1001, 0x0002, 0));
}

TEST(PacketRouterHost, rejects_unreachable_tcp_client_route) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto remote = std::make_shared<IP4Address>();
    remote->set_ip_address("192.0.2.99");
    remote->set_port_address(40650);
    remote->set_reliable(true);

    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0, remote, 35689));
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, tracks_and_removes_internal_connection_before_registration) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto server_address = std::make_shared<IP4Address>();
    server_address->set_reliable(true);
    server_address->set_port_address(35690);
    auto client_address = std::make_shared<IP4Address>();
    client_address->set_reliable(true);
    client_address->set_port_address(35691);
    auto server = std::make_shared<EndpointTCPServer<PacketRouterHost>>(&router);
    auto client = std::make_shared<EndpointTCPClient<PacketRouterHost>>(&router);
    server->set_socket(std::make_shared<Socket>(server_address));
    client->set_socket(std::make_shared<Socket>(client_address, server_address));
    router.local_receiver_ = server;
    server->add_client_endpoint(client->get_socket()->get_socket_fd());

    router.on_connect(server, client);
    EXPECT_EQ(router.connected_endpoints_.count(client->get_socket()->get_socket_fd()), 1U);
    router.on_disconnect(client);
    EXPECT_EQ(router.connected_endpoints_.count(client->get_socket()->get_socket_fd()), 0U);
}

TEST(PacketRouterHost, cleans_registered_application_on_disconnect) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto server_address = std::make_shared<IP4Address>();
    server_address->set_reliable(true);
    server_address->set_port_address(35694);
    auto client_address = std::make_shared<IP4Address>();
    client_address->set_reliable(true);
    client_address->set_port_address(35695);
    auto server = std::make_shared<EndpointTCPServer<PacketRouterHost>>(&router);
    auto client = std::make_shared<EndpointTCPClient<PacketRouterHost>>(&router);
    server->set_socket(std::make_shared<Socket>(server_address));
    client->set_socket(std::make_shared<Socket>(client_address, server_address));
    router.local_receiver_ = server;
    router.local_applications_[0x0005].sender = std::make_shared<CapturingEndpoint>();
    router.local_applications_[0x0005].receiver = std::make_shared<CapturingEndpoint>();
    server->add_client_endpoint(client->get_socket()->get_socket_fd());
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));
    RoutingConnectionInfo connection;
    connection.endpoint = client;
    connection.is_internal = true;
    connection.app_id = 0x0005;
    client->increase_reference_count();
    router.connected_endpoints_[client->get_socket()->get_socket_fd()] = connection;

    router.on_disconnect(client);

    EXPECT_EQ(router.local_applications_.count(0x0005), 0U);
    EXPECT_EQ(router.connected_endpoints_.count(client->get_socket()->get_socket_fd()), 0U);
    EXPECT_EQ(router.registered_service_info_.count(0x1001), 0U);
}

TEST(PacketRouterHost, tracks_and_removes_external_tcp_connection) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto server_source = std::make_shared<IP4Address>();
    server_source->set_reliable(true);
    server_source->set_port_address(35692);
    auto server_destination = std::make_shared<IP4Address>();
    server_destination->set_ip_address("192.0.2.100");
    server_destination->set_reliable(true);
    server_destination->set_port_address(40692);
    auto client_source = std::make_shared<IP4Address>();
    client_source->set_reliable(true);
    client_source->set_port_address(35693);
    auto server = std::make_shared<EndpointTCPServer<PacketRouterHost>>(&router);
    auto client = std::make_shared<EndpointTCPClient<PacketRouterHost>>(&router);
    server->set_socket(std::make_shared<Socket>(server_source, server_destination));
    client->set_socket(std::make_shared<Socket>(client_source, server_destination));
    client->set_server_endpoint(server);
    router.connected_endpoints_[server->get_socket()->get_socket_fd()].endpoint = server;
    router.connected_endpoints_[server->get_socket()->get_socket_fd()].app_id = 0x0007;

    router.on_connect(server, client);
    ASSERT_EQ(router.connected_endpoints_.count(client->get_socket()->get_socket_fd()), 1U);
    router.on_disconnect(client);
    EXPECT_EQ(router.connected_endpoints_.count(client->get_socket()->get_socket_fd()), 0U);
    router.on_disconnect(server);
    EXPECT_EQ(router.connected_endpoints_.count(server->get_socket()->get_socket_fd()), 0U);
}

TEST(PacketRouterHost, adds_and_removes_external_udp_subscription) {
    ScopedPacketRouterConfig config_file;
    auto service_router = std::make_shared<PacketRouterServiceFake>();
    ServiceManager manager("packet-router-test", config_file.path(), service_router);
    PacketRouterHost router(&manager);

    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005, nullptr, 0, nullptr, 35683));
    auto subscriber = std::make_shared<IP4Address>();
    subscriber->set_ip_address("192.0.2.60");
    subscriber->set_port_address(40610);
    subscriber->set_reliable(false);

    router.add_subscribe_route(0x1001, 0x0002, 0x0003, 0, subscriber, 0x0004);
    ASSERT_EQ(router.registered_service_info_[0x1001][0x0002].eventinfos[0x0003].size(), 1U);
    router.remove_subscribe_route(0x1001, 0x0002, 0x0003, 0, subscriber);
    EXPECT_TRUE(router.registered_service_info_[0x1001][0x0002].eventinfos.empty());
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, finds_provider_endpoint_by_local_address) {
    ScopedPacketRouterConfig config_file;
    auto service_router = std::make_shared<PacketRouterServiceFake>();
    ServiceManager manager("packet-router-test", config_file.path(), service_router);
    PacketRouterHost router(&manager);

    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005, nullptr, 0, nullptr, 35684));
    auto source = router.registered_service_info_[0x1001][0x0002].udpendpoint->get_socket()->get_src_address();

    EXPECT_NE(router.find_endpoint(source), nullptr);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, finds_client_endpoint_with_matching_source_and_destination) {
    ScopedPacketRouterConfig config_file;
    auto service_router = std::make_shared<PacketRouterServiceFake>();
    ServiceManager manager("packet-router-test", config_file.path(), service_router);
    PacketRouterHost router(&manager);
    auto remote = std::make_shared<IP4Address>();
    remote->set_ip_address("192.0.2.80");
    remote->set_port_address(40630);
    remote->set_reliable(false);

    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0, nullptr, 0, remote, 35685));
    auto client_endpoint = router.registered_service_info_[0x1001][0x0002].udpendpoint;
    ASSERT_NE(client_endpoint, nullptr);
    auto source = client_endpoint->get_socket()->get_src_address();
    auto destination = client_endpoint->get_socket()->get_dst_address();

    EXPECT_EQ(router.find_endpoint(source, destination), client_endpoint);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, sends_error_response_to_socketless_internal_endpoint) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto receiver = std::make_shared<CapturingEndpoint>();
    auto endpoint = std::make_shared<CapturingEndpoint>();
    endpoint->set_app_id(0x0005);
    router.local_applications_[0x0005].sender = receiver;
    router.set_instance_id(0x1001, 0x0002, static_cast<std::uint16_t>(0x0005));

    auto request = MessageBuilder::create_request_message(0x1001, 0x0001, 0x01);
    request->set_instance_id(0x0002);
    request->set_request_id(0x00050001);
    std::uint8_t buffer[128] = {};
    std::uint32_t length = 0;
    MessageBuilder::build_byte_stream(buffer, &length, *request);

    router.send_error(endpoint, buffer, request->get_message_id(), SOMEIP_RETURN_CODE::E_UNKNOWN_SERVICE);

    ASSERT_EQ(endpoint->message_count, 1);
    ASSERT_EQ(endpoint->last_message.size(), SOMEIP_HEADER::SIZE + 2U);
    EXPECT_EQ(endpoint->last_message[SOMEIP_HEADER::POS::MESSAGETYPE], SOMEIP_MESSAGE_TYPE::ERROR);
    EXPECT_EQ(endpoint->last_message[SOMEIP_HEADER::POS::RETURNCODE], SOMEIP_RETURN_CODE::E_UNKNOWN_SERVICE);
}

TEST(PacketRouterHost, handles_unknown_subscription_and_multicast_connection_paths) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("239.0.0.2");
    address->set_port_address(40631);
    address->set_reliable(false);

    router.remove_subscribe_route(0x1001, 0x0002, 0x0003, 0x0005);
    router.remove_subscribe_route(0x1001, 0x0002, 0x0003, 0, address);
    EXPECT_EQ(router.find_connection(0x1001, 0x0002, address), -1);
}

TEST(PacketRouterHost, registers_and_removes_connected_endpoint_bookkeeping) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_reliable(false);
    address->set_port_address(40632);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(address));
    const auto fd = endpoint->get_socket()->get_socket_fd();

    router.register_connected_endpoint(endpoint, 0x0005);
    ASSERT_EQ(router.connected_endpoints_.count(fd), 1U);
    router.remove_connected_endpoint(endpoint);
    EXPECT_EQ(router.connected_endpoints_.count(fd), 0U);
}

TEST(PacketRouterHost, sends_expired_magic_cookie_for_external_tcp_endpoint) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    int sockets[2] = {};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
    auto destination = std::make_shared<IP4Address>();
    destination->set_ip_address("192.0.2.95");
    destination->set_reliable(true);
    destination->set_port_address(40633);
    auto endpoint = std::make_shared<EndpointTCPClient<PacketRouterHost>>(&router);
    endpoint->set_socket(std::make_shared<Socket>(sockets[0], destination));
    endpoint->set_magic_cookie_enabled(true);
    endpoint->set_last_cookie_sent_time(std::chrono::system_clock::now() - std::chrono::seconds(20));
    RoutingConnectionInfo connection;
    connection.endpoint = endpoint;
    connection.is_internal = false;
    connection.serverfd = 1;
    router.connected_endpoints_[sockets[0]] = connection;

    router.check_and_send_magic_cookies();

    EXPECT_GT(endpoint->get_last_cookie_sent_time(), std::chrono::system_clock::now() - std::chrono::milliseconds(500));
    ::close(sockets[1]);
}

TEST(PacketRouterHost, ignores_disconnect_for_unknown_endpoint) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_reliable(false);
    address->set_port_address(40634);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(address));

    router.on_disconnect(endpoint);
}

TEST(PacketRouterHost, rejects_application_control_message_on_unknown_connection) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_reliable(false);
    address->set_port_address(40635);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(address));
    auto message = MessageBuilder::create<SOMEIPSD>();
    message->set_request_id(0x00050001);
    SDOption option(SOMEIP_SD_OPTION::CONFIGURATION::TYPEID);
    option.set_configuration(SOMEIP_APPLICATION_NAME, "unknown");
    message->options().push_back(option);

    router.on_application_control_message(endpoint, message);
}

TEST(PacketRouterHost, registers_application_control_message_from_known_connection) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    const auto app_id = static_cast<std::uint16_t>(0x0a00 | (::getpid() & 0x00ff));
    auto application_server = EndpointUtils::create_local_socket<TCPServerSocket>(app_id, true);
    ASSERT_NE(application_server, nullptr);
    ASSERT_EQ(application_server->listen(), 0);
    const std::string socket_path = application_server->get_src_address()->get_file_path();

    auto address = std::make_shared<IP4Address>();
    address->set_reliable(false);
    address->set_port_address(40636);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(address));
    const auto fd = endpoint->get_socket()->get_socket_fd();
    router.connected_endpoints_[fd].endpoint = endpoint;

    auto message = MessageBuilder::create<SOMEIPSD>();
    message->set_request_id(static_cast<std::uint32_t>(app_id) << 16);
    SDOption option(SOMEIP_SD_OPTION::CONFIGURATION::TYPEID);
    option.set_configuration(SOMEIP_APPLICATION_NAME, "registered-app");
    message->options().push_back(option);

    router.on_application_control_message(endpoint, message);

    ASSERT_EQ(router.local_applications_.count(app_id), 1U);
    EXPECT_EQ(router.local_applications_[app_id].name, "registered-app");
    EXPECT_EQ(router.connected_endpoints_[fd].app_id, app_id);
    EXPECT_NE(router.local_applications_[app_id].sender, nullptr);

    router.connected_endpoints_.erase(fd);
    router.local_applications_.erase(app_id);
    application_server->close_socket();
    ::unlink(socket_path.c_str());
    ::unlink((socket_path + ".lock").c_str());
}

TEST(PacketRouterHost, forwards_external_response_to_saved_external_request_endpoint) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto service_receiver = std::make_shared<CapturingEndpoint>();
    auto request_endpoint = std::make_shared<CapturingEndpoint>();
    request_endpoint->set_app_id(0x0005);
    router.local_applications_[0x0005].sender = service_receiver;
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));

    auto source = std::make_shared<IP4Address>();
    source->set_ip_address("192.0.2.90");
    source->set_port_address(40640);
    source->set_reliable(false);
    request_endpoint->set_socket(std::make_shared<Socket>(source));
    request_endpoint->get_sender_address()->set_ip_address("192.0.2.90");
    request_endpoint->get_sender_address()->set_port_address(40640);
    router.set_instance_id(0x1001, 0x0002, source);

    std::uint8_t request[32] = {};
    router.on_external_request(request_endpoint, request, SOMEIP_HEADER::SIZE, 0x10010001, 0x00090001, false);
    std::uint8_t response[32] = {};
    router.on_external_response(request_endpoint, response, SOMEIP_HEADER::SIZE, 0x10010001, 0x00090001);

    ASSERT_EQ(service_receiver->message_count, 1);
    ASSERT_EQ(request_endpoint->message_count, 1);
    EXPECT_EQ(request_endpoint->last_message.size(), 18U);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, forwards_internal_request_to_external_udp_service) {
    ScopedPacketRouterConfig config_file;
    auto service_router = std::make_shared<PacketRouterServiceFake>();
    ServiceManager manager("packet-router-test", config_file.path(), service_router);
    PacketRouterHost router(&manager);
    auto remote = std::make_shared<IP4Address>();
    remote->set_ip_address("192.0.2.91");
    remote->set_port_address(40641);
    remote->set_reliable(false);
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0, nullptr, 0, remote, 35687));

    auto endpoint = router.registered_service_info_[0x1001][0x0002].udpendpoint;
    std::uint8_t message[32] = {};
    router.on_internal_request(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010001, 0x00050001, true);

    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, forwards_internal_notification_to_external_udp_subscriber) {
    ScopedPacketRouterConfig config_file;
    auto service_router = std::make_shared<PacketRouterServiceFake>();
    ServiceManager manager("packet-router-test", config_file.path(), service_router);
    PacketRouterHost router(&manager);
    auto remote = std::make_shared<IP4Address>();
    remote->set_ip_address("192.0.2.92");
    remote->set_port_address(40642);
    remote->set_reliable(false);
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0, nullptr, 0, remote, 35688));
    router.add_subscribe_route(0x1001, 0x0002, 0x0003, 0, remote, 0x0004);

    auto endpoint = router.registered_service_info_[0x1001][0x0002].udpendpoint;
    std::uint8_t message[32] = {};
    router.on_internal_notification(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010003, 0x00000001);

    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, forwards_internal_request_to_local_service_endpoint) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto receiver = std::make_shared<CapturingEndpoint>();
    router.local_applications_[0x0005].sender = receiver;
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));

    auto endpoint = std::make_shared<EndpointBase>();
    endpoint->set_app_id(0x0005);
    std::uint8_t message[32] = {};
    router.on_internal_request(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010001, 0x00050001, true);

    ASSERT_EQ(receiver->message_count, 1);
    EXPECT_EQ(receiver->last_message.size(), 18U);
    router.on_internal_request(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010001, 0x00050001, false);
    EXPECT_EQ(receiver->message_count, 2);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, forwards_internal_response_to_saved_request_endpoint) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto receiver = std::make_shared<CapturingEndpoint>();
    router.request_map_[0x0005].endpoint = receiver;
    auto endpoint = std::make_shared<EndpointBase>();
    std::uint8_t message[32] = {};

    router.on_internal_response(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010001, 0x00050001);

    ASSERT_EQ(receiver->message_count, 1);
    EXPECT_EQ(receiver->last_message.size(), 18U);
}

TEST(PacketRouterHost, forwards_internal_notification_to_subscriber) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto receiver = std::make_shared<CapturingEndpoint>();
    router.local_applications_[0x0005].sender = receiver;
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));
    router.add_subscribe_route(0x1001, 0x0002, 0x0003, 0x0005);

    auto endpoint = std::make_shared<EndpointBase>();
    endpoint->set_app_id(0x0005);
    std::uint8_t message[32] = {};
    router.on_internal_notification(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010003, 0x00000001);

    ASSERT_EQ(receiver->message_count, 1);
    EXPECT_EQ(receiver->last_message.size(), 18U);
    router.on_internal_notification(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010003, 0x00050001);
    EXPECT_EQ(receiver->message_count, 2);
    router.on_internal_notification(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010003, 0x00060001);
    EXPECT_EQ(receiver->message_count, 2);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, uses_any_event_subscription_for_unlisted_notification) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto receiver = std::make_shared<CapturingEndpoint>();
    router.local_applications_[0x0005].sender = receiver;
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));
    router.add_subscribe_route(0x1001, 0x0002, SOMEIP_DEFAULT_ANY_EVENT, 0x0005);

    auto endpoint = std::make_shared<EndpointBase>();
    endpoint->set_app_id(0x0005);
    std::uint8_t message[32] = {};
    router.on_internal_notification(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010003, 0x00000001);

    EXPECT_EQ(receiver->message_count, 1);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, forwards_external_request_to_internal_service) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto receiver = std::make_shared<CapturingEndpoint>();
    router.local_applications_[0x0005].sender = receiver;
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));

    auto source = std::make_shared<IP4Address>();
    source->set_ip_address("192.0.2.50");
    source->set_port_address(40600);
    source->set_reliable(false);
    router.set_instance_id(0x1001, 0x0002, source);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(source));
    std::uint8_t message[32] = {};

    router.on_external_request(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010001, 0x00090001, true);

    ASSERT_EQ(receiver->message_count, 1);
    EXPECT_EQ(receiver->last_message.size(), 10U);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, forwards_external_notification_to_internal_subscriber) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto receiver = std::make_shared<CapturingEndpoint>();
    router.local_applications_[0x0005].sender = receiver;
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));

    auto source = std::make_shared<IP4Address>();
    source->set_ip_address("192.0.2.51");
    source->set_port_address(40601);
    source->set_reliable(false);
    router.set_instance_id(0x1001, 0x0002, source);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(source));
    router.add_subscribe_route(0x1001, 0x0002, 0x0003, 0x0005);
    std::uint8_t message[32] = {};

    router.on_external_notification(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010003, 0x00090001);

    ASSERT_EQ(receiver->message_count, 1);
    EXPECT_EQ(receiver->last_message.size(), 18U);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, ignores_external_request_with_error_return_code) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto endpoint = std::make_shared<EndpointBase>();
    std::uint8_t message[32] = {};
    message[SOMEIP_HEADER::POS::RETURNCODE] = 0x01;

    router.on_external_request(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010001, 0x00050001, false);
}

TEST(PacketRouterHost, ignores_external_response_without_saved_request) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto endpoint = std::make_shared<EndpointBase>();
    std::uint8_t message[32] = {};

    router.on_external_response(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010001, 0x00050001);
}

TEST(PacketRouterHost, ignores_external_notification_without_service_info) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto endpoint = std::make_shared<EndpointBase>();
    std::uint8_t message[16] = {};

    router.on_external_notification(endpoint, message, SOMEIP_HEADER::SIZE, 0x10010002, 0x00050001);
}

TEST(PacketRouterHost, reboot_route_removes_matching_request_state) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("192.0.2.70");
    address->set_port_address(40620);
    address->set_reliable(false);
    router.request_map_[0x0005].targeaddr = address;
    router.request_map_[0x0006].targeaddr = std::make_shared<IP4Address>();
    router.request_map_[0x0006].targeaddr->set_ip_address("192.0.2.71");

    router.reboot_route(address);

    EXPECT_EQ(router.request_map_.count(0x0005), 0U);
    EXPECT_EQ(router.request_map_.count(0x0006), 1U);
}

TEST(PacketRouterHost, dispatches_malformed_internal_someip_without_crashing) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("192.0.2.70");
    address->set_port_address(40620);
    address->set_reliable(false);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(address));
    router.connected_endpoints_[endpoint->get_socket()->get_socket_fd()].is_internal = true;
    std::uint8_t message[] = {0x00};

    router.on_message(endpoint, message, sizeof(message));
}

TEST(PacketRouterHost, rejects_external_sd_message_from_unknown_port) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("192.0.2.71");
    address->set_port_address(40621);
    address->set_reliable(false);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(address));
    endpoint->get_sender_address()->set_ip_address("192.0.2.71");
    endpoint->get_sender_address()->set_port_address(40621);
    auto message = MessageBuilder::create<SOMEIPSD>();
    SDEntry entry(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
    entry.set_service_id(0x1001);
    entry.set_instance_id(0x0002);
    entry.set_major_version(0x01);
    entry.set_minor_version(0x00000003);
    entry.set_ttl(SOMEIP_DEFAULT_TTL_ON);
    MessageComposer::add_entry(message, &entry);
    std::uint8_t data[256] = {};
    std::uint32_t length = 0;
    MessageBuilder::build_byte_stream(data, &length, *message);

    router.on_message(endpoint, data, length);
}

TEST(PacketRouterHost, rejects_external_sd_message_from_local_address) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("127.0.0.1");
    address->set_port_address(30490);
    address->set_reliable(false);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(address));
    endpoint->get_sender_address()->set_ip_address("127.0.0.1");
    endpoint->get_sender_address()->set_port_address(30490);
    auto message = MessageBuilder::create<SOMEIPSD>();
    SDEntry entry(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
    entry.set_service_id(0x1001);
    entry.set_instance_id(0x0002);
    entry.set_major_version(0x01);
    entry.set_minor_version(0x00000003);
    entry.set_ttl(SOMEIP_DEFAULT_TTL_ON);
    MessageComposer::add_entry(message, &entry);
    std::uint8_t data[256] = {};
    std::uint32_t length = 0;
    MessageBuilder::build_byte_stream(data, &length, *message);

    router.on_message(endpoint, data, length);
}

TEST(PacketRouterHost, handles_service_state_dump_trigger_validation) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("192.0.2.74");
    address->set_port_address(40624);
    address->set_reliable(false);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(address));
    std::uint8_t message[32] = {};
    message[0] = 0xff;
    message[1] = 0xff;
    message[2] = 0x81;
    message[3] = 0x00;
    message[24] = 0xf1;
    message[25] = 0xf2;
    message[14] = 0x01;
    router.on_message(endpoint, message, sizeof(message));
    message[12] = 0;
    message[13] = 0;
    message[14] = 0x02;
    message[15] = 0;
    router.on_message(endpoint, message, sizeof(message));
}

TEST(PacketRouterHost, dispatches_internal_someip_message_types) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("192.0.2.75");
    address->set_port_address(40625);
    address->set_reliable(false);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(address));
    router.connected_endpoints_[endpoint->get_socket()->get_socket_fd()].is_internal = true;

    auto request = MessageBuilder::create_request_message(0x1001, 0x0001, 0x01);
    request->set_request_id(0x00050001);
    auto response = MessageBuilder::create_response_message(*request);
    auto notification = MessageBuilder::create_notification_message(0x1001, 0x0002);
    std::vector<std::shared_ptr<MessageSOMEIP>> messages = {request, response, notification};
    for (const auto& message : messages) {
        std::uint8_t data[128] = {};
        std::uint32_t length = 0;
        MessageBuilder::build_byte_stream(data, &length, *message);
        router.on_message(endpoint, data, length);
    }
}

TEST(PacketRouterHost, rejects_malformed_external_datagram_without_response) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto endpoint = std::make_shared<EndpointBase>();
    std::uint8_t malformed[] = {0x00};

    router.on_external_message(endpoint, malformed, sizeof(malformed));
}

TEST(PacketRouterHost, sends_error_for_wrong_external_protocol_version) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("192.0.2.72");
    address->set_port_address(40622);
    address->set_reliable(false);
    auto endpoint = std::make_shared<CapturingEndpoint>();
    endpoint->set_socket(std::make_shared<Socket>(address));
    endpoint->get_sender_address()->set_ip_address("192.0.2.72");
    endpoint->get_sender_address()->set_port_address(40622);

    auto message = MessageBuilder::create_request_message(0x1001, 0x0001, 0x01);
    message->set_request_id(0x00090001);
    message->set_protocol_version(0x02);
    std::uint8_t data[128] = {};
    std::uint32_t length = 0;
    MessageBuilder::build_byte_stream(data, &length, *message);

    router.on_external_message(endpoint, data, length);

    ASSERT_EQ(endpoint->message_count, 1);
    EXPECT_EQ(endpoint->last_message[SOMEIP_HEADER::POS::MESSAGETYPE], SOMEIP_MESSAGE_TYPE::ERROR);
}

TEST(PacketRouterHost, sends_error_for_unknown_external_message_type) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto address = std::make_shared<IP4Address>();
    address->set_ip_address("192.0.2.73");
    address->set_port_address(40623);
    address->set_reliable(false);
    auto endpoint = std::make_shared<CapturingEndpoint>();
    endpoint->set_socket(std::make_shared<Socket>(address));
    endpoint->get_sender_address()->set_ip_address("192.0.2.73");
    endpoint->get_sender_address()->set_port_address(40623);

    auto message = MessageBuilder::create<SOMEIP>();
    message->set_message_id(0x10010001);
    message->set_request_id(0x00090001);
    message->set_message_type(0x7f);
    std::uint8_t data[128] = {};
    std::uint32_t length = 0;
    MessageBuilder::build_byte_stream(data, &length, *message);

    router.on_external_message(endpoint, data, length);

    ASSERT_EQ(endpoint->message_count, 1);
    EXPECT_EQ(endpoint->last_message[SOMEIP_HEADER::POS::MESSAGETYPE], SOMEIP_MESSAGE_TYPE::ERROR);
}

TEST(PacketRouterHost, dispatches_valid_external_request_through_message_parser) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto receiver = std::make_shared<CapturingEndpoint>();
    router.local_applications_[0x0005].sender = receiver;
    ASSERT_TRUE(router.add_route(0x1001, 0x0002, 0x0005));
    auto source = std::make_shared<IP4Address>();
    source->set_ip_address("192.0.2.76");
    source->set_port_address(40626);
    source->set_reliable(false);
    router.set_instance_id(0x1001, 0x0002, source);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(source));
    endpoint->get_sender_address()->set_ip_address("192.0.2.76");
    endpoint->get_sender_address()->set_port_address(40626);
    auto request = MessageBuilder::create_request_message(0x1001, 0x0001, 0x01);
    request->set_request_id(0x00090001);
    std::uint8_t data[128] = {};
    std::uint32_t length = 0;
    MessageBuilder::build_byte_stream(data, &length, *request);

    router.on_external_message(endpoint, data, length);

    ASSERT_EQ(receiver->message_count, 1);
    EXPECT_EQ(receiver->last_message.size(), 18U);
    router.remove_route(0x1001, 0x0002);
}

TEST(PacketRouterHost, ignores_non_request_or_null_messages_in_error_builder) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    auto endpoint = std::make_shared<CapturingEndpoint>();
    std::uint8_t response[32] = {};
    response[SOMEIP_HEADER::POS::MESSAGETYPE] = SOMEIP_MESSAGE_TYPE::RESPONSE;

    router.send_error(endpoint, nullptr, 0x10010001, SOMEIP_RETURN_CODE::E_NOT_OK);
    router.send_error(endpoint, response, 0x10010001, SOMEIP_RETURN_CODE::E_NOT_OK);

    EXPECT_EQ(endpoint->message_count, 0);
}

TEST(PacketRouterHost, handles_external_response_without_endpoint_state) {
    auto manager = make_service_manager();
    PacketRouterHost router(manager.get());
    router.request_map_[0x0005].endpoint = nullptr;
    auto endpoint = std::make_shared<EndpointBase>();
    std::uint8_t response[32] = {};

    router.on_external_response(endpoint, response, SOMEIP_HEADER::SIZE, 0x10010001, 0x00050001);
}
