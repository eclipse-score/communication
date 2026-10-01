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

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <message/MessageComposer.h>
#include <endpoint/EndpointBase.h>
#include <mutex>
#include <packetrouter/ServiceRouter.h>
#define private public
#include <runtime/ServiceManager.h>
#undef private
#include <socket/Socket.h>
#include <unistd.h>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

class FakeServiceRouter final : public ServiceRouter {
public:
    void init() override {
        initialized = true;
    }

    void start() override {
        started = true;
    }
    void stop() override {
        stopped = true;
    }

    std::shared_ptr<Multiplexer> get_multiplexer() override {
        return nullptr;
    }

    std::shared_ptr<Endpoint> get_service_discovery_multicast_endpoint() override {
        return service_discovery_endpoint;
    }

    std::shared_ptr<Endpoint> get_service_discovery_unicast_endpoint() override {
        return nullptr;
    }

    std::shared_ptr<Address> make_address() override {
        return std::make_shared<IP4Address>();
    }

    void set_instance_id(std::uint16_t, std::uint16_t, std::uint16_t) override {
        ++set_instance_id_count;
    }

    bool add_route(std::uint16_t, std::uint16_t, std::uint16_t, std::shared_ptr<Address>, std::uint16_t,
                   std::shared_ptr<Address>, std::uint16_t, std::uint8_t,
                   std::shared_ptr<std::vector<std::shared_ptr<Address>>>) override {
        {
            std::lock_guard<std::mutex> lock(internal_message_mutex);
            ++add_route_count;
        }
        internal_message_condition.notify_all();
        return add_route_result;
    }

    void remove_route(std::uint16_t, std::uint16_t) override {
        ++remove_route_count;
    }

    std::int32_t find_connection(std::uint16_t, std::uint16_t, std::shared_ptr<Address>) override {
        return 0;
    }

    void add_subscribe_route(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, std::shared_ptr<Address>,
                             std::uint16_t) override {}
    void remove_subscribe_route(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t,
                                std::shared_ptr<Address>) override {}
    void remove_subscribe_route(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t) override {}

    void send_internal_message(std::uint8_t* message, std::size_t length, std::uint16_t target_app_id) override {
        {
            std::lock_guard<std::mutex> lock(internal_message_mutex);
            internal_messages.emplace_back(target_app_id, std::vector<std::uint8_t>(message, message + length));
        }
        internal_message_condition.notify_all();
    }

    void send_external_sd_message(std::uint8_t* message, std::size_t length, std::shared_ptr<Address> address,
                                  bool multicast) override {
        captured_message.assign(message, message + length);
        captured_address = std::move(address);
        captured_multicast = multicast;
    }

    void check_and_send_magic_cookies() override {}
    void reboot_route(std::shared_ptr<Address>) override {}

    bool initialized = false;
    bool started = false;
    bool stopped = false;
    bool add_route_result = true;
    int add_route_count = 0;
    int remove_route_count = 0;
    int set_instance_id_count = 0;
    std::vector<std::uint8_t> captured_message;
    std::shared_ptr<Address> captured_address;
    bool captured_multicast = false;
    std::shared_ptr<Endpoint> service_discovery_endpoint;
    std::vector<std::pair<std::uint16_t, std::vector<std::uint8_t>>> internal_messages;
    std::mutex internal_message_mutex;
    std::condition_variable internal_message_condition;

    bool wait_for_internal_messages(std::size_t count) {
        std::unique_lock<std::mutex> lock(internal_message_mutex);
        return internal_message_condition.wait_for(lock, std::chrono::seconds(1),
                                                   [&] { return internal_messages.size() >= count; });
    }

    bool wait_for_add_routes(int count) {
        std::unique_lock<std::mutex> lock(internal_message_mutex);
        return internal_message_condition.wait_for(lock, std::chrono::seconds(1),
                                                   [&] { return add_route_count >= count; });
    }
};

class ScopedServiceManagerConfig final {
public:
    ScopedServiceManagerConfig() : path_("/tmp/lgsomeip-service-manager-test-" + std::to_string(::getpid()) + ".json") {
        std::ofstream file(path_);
        file << R"({
            "unicast": "127.0.0.1",
            "applications": [],
            "services": [
                {
                    "service": "0x1001",
                    "instance": "0x0002",
                    "reliable": { "port": "30511" },
                    "unreliable": "30509",
                    "is-provider": "false",
                    "events": [
                        { "event": "0x0003", "is_field": "false" }
                    ],
                    "eventgroups": [
                        { "eventgroup": "0x0004", "events": [ "0x0003" ] }
                    ]
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

    ~ScopedServiceManagerConfig() {
        std::remove(path_.c_str());
    }

    const std::string& path() const {
        return path_;
    }

private:
    std::string path_;
};

std::shared_ptr<EndpointBase> make_service_discovery_endpoint(FakeServiceRouter& router) {
    auto sender_address = std::make_shared<IP4Address>();
    sender_address->set_ip_address("192.0.2.1");
    sender_address->set_port_address(30490);
    sender_address->set_reliable(false);
    auto endpoint = std::make_shared<EndpointBase>(std::make_shared<Socket>(sender_address));
    endpoint->get_sender_address()->set_ip_address("192.0.2.1");
    router.service_discovery_endpoint = endpoint;
    return endpoint;
}

std::shared_ptr<MessageSD> make_service_message(std::uint8_t type, std::uint16_t service, std::uint16_t instance,
                                                std::uint8_t major, std::uint32_t minor, std::uint32_t ttl,
                                                std::uint32_t request_id) {
    auto message = MessageBuilder::create<SOMEIPSD>();
    message->set_request_id(request_id);
    SDEntry entry(type);
    entry.set_service_id(service);
    entry.set_instance_id(instance);
    entry.set_major_version(major);
    entry.set_minor_version(minor);
    entry.set_ttl(ttl);
    MessageComposer::add_entry(message, &entry);
    return message;
}

TEST(ServiceManager, forwards_external_find_service_as_sd_message) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    manager.init();
    manager.send_external_find_service(0x1001, 0x0002, 3, 0x01, 0x00000004);

    ASSERT_TRUE(router->initialized);
    ASSERT_FALSE(router->captured_message.empty());
    EXPECT_EQ(router->captured_address, nullptr);
    EXPECT_TRUE(router->captured_multicast);

    auto message = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(MessageBuilder::build_message(*message, router->captured_message.data(),
                                              static_cast<std::uint32_t>(router->captured_message.size())));
    ASSERT_EQ(message->entries().size(), 1U);
    EXPECT_EQ(message->entry(0).get_type(), SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
    EXPECT_EQ(message->entry(0).get_service_id(), 0x1001);
    EXPECT_EQ(message->entry(0).get_instance_id(), 0x0002);
    EXPECT_EQ(message->entry(0).get_major_version(), 0x01);
    EXPECT_EQ(message->entry(0).get_minor_version(), 0x00000004U);
    EXPECT_EQ(message->entry(0).get_ttl(), 3U);
}

TEST(ServiceManager, uses_injected_router_for_lifecycle) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    manager.init();
    manager.start();
    manager.stop();

    EXPECT_TRUE(router->initialized);
    EXPECT_TRUE(router->started);
    EXPECT_TRUE(router->stopped);
    EXPECT_EQ(manager.get_application_id(), 0);
    EXPECT_EQ(manager.get_application_name(), "service-test");
}

TEST(ServiceManager, on_disconnected_application_without_registered_services_is_noop) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    EXPECT_NO_THROW(manager.on_disconnected_application(0x0007));
}

TEST(ServiceManager, on_disconnected_service_without_registered_services_is_noop) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    auto address = std::make_shared<IP4Address>();
    EXPECT_NO_THROW(manager.on_disconnected_service(address));
}

TEST(ServiceManager, send_external_find_service_all_builds_sd_message_for_each_entry) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    RequestedService first_request(0x0005, 0x01, 0x00000002);
    RequestedService second_request(0x0006, 0x02, 0x00000003);
    std::map<std::uint16_t, std::map<std::uint16_t, RequestedService*>> find_list;
    find_list[0x1001][0x0002] = &first_request;
    find_list[0x1002][0x0003] = &second_request;

    manager.send_external_find_service_all(find_list);

    ASSERT_FALSE(router->captured_message.empty());
    auto message = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(MessageBuilder::build_message(*message, router->captured_message.data(),
                                              static_cast<std::uint32_t>(router->captured_message.size())));
    ASSERT_EQ(message->entries().size(), 2U);
    EXPECT_EQ(message->entry(0).get_type(), SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
    EXPECT_EQ(message->entry(0).get_service_id(), 0x1001);
    EXPECT_EQ(message->entry(0).get_instance_id(), 0x0002);
    EXPECT_EQ(message->entry(0).get_major_version(), 0x01);
    EXPECT_EQ(message->entry(0).get_minor_version(), 0x00000002U);
    EXPECT_EQ(message->entry(1).get_type(), SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);
    EXPECT_EQ(message->entry(1).get_service_id(), 0x1002);
    EXPECT_EQ(message->entry(1).get_instance_id(), 0x0003);
    EXPECT_EQ(message->entry(1).get_major_version(), 0x02);
    EXPECT_EQ(message->entry(1).get_minor_version(), 0x00000003U);
}

TEST(ServiceManager, forwards_internal_offer_after_find_and_removes_it_on_stop) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00050001));
    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00070001));

    ASSERT_TRUE(router->wait_for_internal_messages(1));
    ASSERT_EQ(router->internal_messages.front().first, 0x0005);
    auto forwarded_offer = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(
        MessageBuilder::build_message(*forwarded_offer, router->internal_messages.front().second.data(),
                                      static_cast<std::uint32_t>(router->internal_messages.front().second.size())));
    ASSERT_EQ(forwarded_offer->entries().size(), 1U);
    EXPECT_EQ(forwarded_offer->entry(0).get_type(), SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    EXPECT_EQ(forwarded_offer->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_ON);
    EXPECT_EQ(router->add_route_count, 1);
    EXPECT_EQ(router->set_instance_id_count, 1);

    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_OFF, 0x00070001));

    ASSERT_TRUE(router->wait_for_internal_messages(2));
    auto stopped_offer = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(
        MessageBuilder::build_message(*stopped_offer, router->internal_messages.back().second.data(),
                                      static_cast<std::uint32_t>(router->internal_messages.back().second.size())));
    ASSERT_EQ(stopped_offer->entries().size(), 1U);
    EXPECT_EQ(stopped_offer->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
    EXPECT_EQ(router->remove_route_count, 1);
}

TEST(ServiceManager, serializes_external_offer_with_tcp_and_udp_options) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    auto tcp = std::make_shared<IP4Address>();
    tcp->set_ip_address("192.0.2.10");
    tcp->set_port_address(30490);
    tcp->set_reliable(true);
    auto udp = std::make_shared<IP4Address>();
    udp->set_ip_address("192.0.2.11");
    udp->set_port_address(30491);
    udp->set_reliable(false);
    AvailableService available(0, true, 0x01, 0x00000003, 3, tcp, udp);
    std::map<std::uint16_t, std::map<std::uint16_t, AvailableService*>> offer_list;
    offer_list[0x1001][0x0002] = &available;

    manager.send_external_offer_service(offer_list, nullptr, 3, true);

    ASSERT_FALSE(router->captured_message.empty());
    auto message = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(MessageBuilder::build_message(*message, router->captured_message.data(),
                                              static_cast<std::uint32_t>(router->captured_message.size())));
    ASSERT_EQ(message->entries().size(), 1U);
    ASSERT_EQ(message->options().size(), 2U);
    EXPECT_EQ(message->entry(0).get_type(), SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    EXPECT_EQ(message->entry(0).get_option1st_count(), 2);
    EXPECT_EQ(message->entry(0).get_ttl(), 3U);
    EXPECT_EQ(message->option(0).get_address_option()->get_port_address(), 30490);
    EXPECT_EQ(message->option(1).get_address_option()->get_port_address(), 30491);
    EXPECT_TRUE(router->captured_multicast);
}

TEST(ServiceManager, missing_service_subscribe_ack_serializes_nack) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    manager.send_subscribe_eventgroup_ack(0x1001, 0x0002, 0x0004, SOMEIP_DEFAULT_TTL_ON, 0x01, 0, nullptr,
                                          "192.0.2.12");

    ASSERT_FALSE(router->captured_message.empty());
    auto message = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(MessageBuilder::build_message(*message, router->captured_message.data(),
                                              static_cast<std::uint32_t>(router->captured_message.size())));
    ASSERT_EQ(message->entries().size(), 1U);
    EXPECT_EQ(message->entry(0).get_type(), SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    EXPECT_EQ(message->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
    EXPECT_FALSE(router->captured_multicast);
    ASSERT_NE(router->captured_address, nullptr);
    EXPECT_EQ(router->captured_address->get_ip_address(), "192.0.2.12");
}

TEST(ServiceManager, internal_subscribe_without_service_sends_nack_to_requester) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);
    auto subscribe = make_service_message(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID, 0x1001, 0x0002, 0x01, 0,
                                          SOMEIP_DEFAULT_TTL_ON, 0x00050001);
    subscribe->entry(0).set_event_group_id(0x0004);

    manager.on_internal_message(subscribe);

    ASSERT_TRUE(router->wait_for_internal_messages(1));
    ASSERT_EQ(router->internal_messages.front().first, 0x0005);
    auto acknowledgement = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(
        MessageBuilder::build_message(*acknowledgement, router->internal_messages.front().second.data(),
                                      static_cast<std::uint32_t>(router->internal_messages.front().second.size())));
    ASSERT_EQ(acknowledgement->entries().size(), 1U);
    EXPECT_EQ(acknowledgement->entry(0).get_type(), SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    EXPECT_EQ(acknowledgement->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
}

TEST(ServiceManager, sends_internal_subscribe_and_ack_for_offered_service) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00050001));
    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00070001));
    ASSERT_TRUE(router->wait_for_internal_messages(1));

    manager.send_subscribe_eventgroup(0x1001, 0x0002, 0x0004, SOMEIP_DEFAULT_TTL_ON, 0x01, 0x0005);
    ASSERT_TRUE(router->wait_for_internal_messages(2));
    ASSERT_EQ(router->internal_messages.back().first, 0x0007);
    auto subscribe = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(
        MessageBuilder::build_message(*subscribe, router->internal_messages.back().second.data(),
                                      static_cast<std::uint32_t>(router->internal_messages.back().second.size())));
    ASSERT_EQ(subscribe->entries().size(), 1U);
    EXPECT_EQ(subscribe->entry(0).get_type(), SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID);
    EXPECT_EQ(subscribe->entry(0).get_event_group_id(), 0x0004);

    manager.send_subscribe_eventgroup_ack(0x1001, 0x0002, 0x0004, SOMEIP_DEFAULT_TTL_ON, 0x01, 0x0005);
    ASSERT_TRUE(router->wait_for_internal_messages(3));
    ASSERT_EQ(router->internal_messages.back().first, 0x0005);
    auto acknowledgement = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(
        MessageBuilder::build_message(*acknowledgement, router->internal_messages.back().second.data(),
                                      static_cast<std::uint32_t>(router->internal_messages.back().second.size())));
    ASSERT_EQ(acknowledgement->entries().size(), 1U);
    EXPECT_EQ(acknowledgement->entry(0).get_type(), SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    EXPECT_EQ(acknowledgement->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_ON);
}

TEST(ServiceManager, handles_internal_subscribe_update_stop_and_ack_lifecycle) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00070001));
    ASSERT_TRUE(router->wait_for_add_routes(1));

    auto subscribe = make_service_message(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID, 0x1001, 0x0002, 0x01, 0,
                                          SOMEIP_DEFAULT_TTL_ON, 0x00050001);
    subscribe->entry(0).set_event_group_id(0x0004);
    manager.on_internal_message(subscribe);
    ASSERT_TRUE(router->wait_for_internal_messages(1));

    manager.on_internal_message(subscribe);
    ASSERT_TRUE(router->wait_for_internal_messages(2));

    auto subscribe_ack = make_service_message(SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID, 0x1001, 0x0002, 0x01, 0,
                                              SOMEIP_DEFAULT_TTL_ON, 0x00050001);
    subscribe_ack->entry(0).set_event_group_id(0x0004);
    manager.on_internal_message(subscribe_ack);
    ASSERT_TRUE(router->wait_for_internal_messages(3));

    subscribe->entry(0).set_ttl(SOMEIP_DEFAULT_TTL_OFF);
    manager.on_internal_message(subscribe);
    ASSERT_TRUE(router->wait_for_internal_messages(4));

    auto stop_ack = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(
        MessageBuilder::build_message(*stop_ack, router->internal_messages.back().second.data(),
                                      static_cast<std::uint32_t>(router->internal_messages.back().second.size())));
    ASSERT_EQ(stop_ack->entries().size(), 1U);
    EXPECT_EQ(stop_ack->entry(0).get_type(), SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    EXPECT_EQ(stop_ack->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
}

TEST(ServiceManager, ignores_internal_offer_when_requested_version_does_not_match) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00050001));
    manager.send_internal_offer_service_all(0x1001, 0x0002, SOMEIP_DEFAULT_TTL_ON, 0x02, 0x00000004);

    EXPECT_TRUE(router->internal_messages.empty());
}

TEST(ServiceManager, removes_internal_find_request_when_ttl_is_zero) {
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", "", router);

    auto find = make_service_message(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID, 0x1001, 0x0002, 0x01, 0x00000003,
                                     SOMEIP_DEFAULT_TTL_ON, 0x00050001);
    manager.on_internal_message(find);
    find->entry(0).set_ttl(SOMEIP_DEFAULT_TTL_OFF);
    manager.on_internal_message(find);

    EXPECT_TRUE(router->internal_messages.empty());
}

TEST(ServiceManager, processes_configured_external_offer_and_stop_offer) {
    ScopedServiceManagerConfig config_file;
    auto router = std::make_shared<FakeServiceRouter>();

    auto endpoint = make_service_discovery_endpoint(*router);

    ServiceManager manager("service-test", config_file.path(), router);
    auto offered_address = std::make_shared<IP4Address>();
    offered_address->set_ip_address("192.0.2.10");
    offered_address->set_port_address(30509);
    offered_address->set_reliable(false);

    auto offer = make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01, 0x00000003,
                                      SOMEIP_DEFAULT_TTL_ON, 0x00000001);
    SDOption option(SOMEIP_SD_OPTION::IP4::TYPEID);
    option.set_address_option(offered_address);
    offer->options().push_back(option);
    offer->entry(0).set_option1st_index(0);
    offer->entry(0).set_option1st_count(1);

    manager.on_external_message(endpoint, offer, false);

    ASSERT_TRUE(router->wait_for_add_routes(1));
    EXPECT_EQ(router->add_route_count, 1);
    EXPECT_EQ(router->remove_route_count, 0);

    auto stop_offer = make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01, 0x00000003,
                                           SOMEIP_DEFAULT_TTL_OFF, 0x00000001);
    stop_offer->options().push_back(option);
    stop_offer->entry(0).set_option1st_index(0);
    stop_offer->entry(0).set_option1st_count(1);
    manager.on_external_message(endpoint, stop_offer, false);

    EXPECT_EQ(router->remove_route_count, 1);
}

TEST(ServiceManager, removes_external_service_when_tcp_peer_disconnects) {
    ScopedServiceManagerConfig config_file;
    auto router = std::make_shared<FakeServiceRouter>();
    auto endpoint = make_service_discovery_endpoint(*router);
    ServiceManager manager("service-test", config_file.path(), router);
    auto offered_address = std::make_shared<IP4Address>();
    offered_address->set_ip_address("192.0.2.20");
    offered_address->set_port_address(40511);
    offered_address->set_reliable(true);
    auto offer = make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01, 0x00000003,
                                      SOMEIP_DEFAULT_TTL_ON, 0x00000001);
    SDOption option(SOMEIP_SD_OPTION::IP4::TYPEID);
    option.set_address_option(offered_address);
    offer->options().push_back(option);
    offer->entry(0).set_option1st_index(0);
    offer->entry(0).set_option1st_count(1);

    manager.on_external_message(endpoint, offer, false);
    ASSERT_TRUE(router->wait_for_add_routes(1));
    manager.on_disconnected_service(offered_address);

    EXPECT_EQ(manager.find_available_service_instance(0x1001, 0x0002), nullptr);
    EXPECT_EQ(router->remove_route_count, 0);
}

TEST(ServiceManager, removes_internal_provider_when_application_disconnects) {
    ScopedServiceManagerConfig config_file;
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", config_file.path(), router);

    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00070001));
    ASSERT_TRUE(router->wait_for_add_routes(1));
    manager.on_disconnected_application(0x0007);

    ASSERT_FALSE(router->captured_message.empty());
    auto stop_offer = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(MessageBuilder::build_message(*stop_offer, router->captured_message.data(),
                                              static_cast<std::uint32_t>(router->captured_message.size())));
    ASSERT_EQ(stop_offer->entries().size(), 1U);
    EXPECT_EQ(stop_offer->entry(0).get_type(), SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    EXPECT_EQ(stop_offer->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
    EXPECT_EQ(manager.find_available_service_instance(0x1001, 0x0002), nullptr);
    EXPECT_EQ(router->remove_route_count, 0);
}

TEST(ServiceManager, external_find_service_returns_offer_for_internal_provider) {
    ScopedServiceManagerConfig config_file;
    auto router = std::make_shared<FakeServiceRouter>();
    auto endpoint = make_service_discovery_endpoint(*router);
    ServiceManager manager("service-test", config_file.path(), router);

    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00070001));
    ASSERT_TRUE(router->wait_for_add_routes(1));

    auto find = make_service_message(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID, 0x1001, 0x0002, 0x01, 0x00000003,
                                     SOMEIP_DEFAULT_TTL_ON, 0x00090001);
    find->set_flag(0x40);
    manager.on_external_message(endpoint, find, false);

    ASSERT_FALSE(router->captured_message.empty());
    auto response = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(MessageBuilder::build_message(*response, router->captured_message.data(),
                                              static_cast<std::uint32_t>(router->captured_message.size())));
    ASSERT_EQ(response->entries().size(), 1U);
    EXPECT_EQ(response->entry(0).get_type(), SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    EXPECT_EQ(response->entry(0).get_service_id(), 0x1001);
    EXPECT_EQ(response->entry(0).get_instance_id(), 0x0002);
    EXPECT_FALSE(router->captured_multicast);
    ASSERT_NE(router->captured_address, nullptr);
    EXPECT_EQ(router->captured_address->get_ip_address(), "192.0.2.1");
}

TEST(ServiceManager, external_subscribe_is_forwarded_to_internal_provider) {
    ScopedServiceManagerConfig config_file;
    auto router = std::make_shared<FakeServiceRouter>();
    auto endpoint = make_service_discovery_endpoint(*router);
    ServiceManager manager("service-test", config_file.path(), router);

    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00070001));
    ASSERT_TRUE(router->wait_for_add_routes(1));

    auto subscriber_address = std::make_shared<IP4Address>();
    subscriber_address->set_ip_address("192.0.2.1");
    subscriber_address->set_port_address(40000);
    subscriber_address->set_reliable(false);
    auto subscribe = make_service_message(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID, 0x1001, 0x0002, 0x01, 0x00000000,
                                          SOMEIP_DEFAULT_TTL_ON, 0x00090001);
    subscribe->entry(0).set_event_group_id(0x0004);
    SDOption option(SOMEIP_SD_OPTION::IP4::TYPEID);
    option.set_address_option(subscriber_address);
    subscribe->options().push_back(option);
    subscribe->entry(0).set_option1st_index(0);
    subscribe->entry(0).set_option1st_count(1);

    manager.on_external_message(endpoint, subscribe, false);

    ASSERT_TRUE(router->wait_for_internal_messages(1));
    ASSERT_EQ(router->internal_messages.back().first, 0x0007);
    auto forwarded_subscribe = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(
        MessageBuilder::build_message(*forwarded_subscribe, router->internal_messages.back().second.data(),
                                      static_cast<std::uint32_t>(router->internal_messages.back().second.size())));
    ASSERT_EQ(forwarded_subscribe->entries().size(), 1U);
    EXPECT_EQ(forwarded_subscribe->entry(0).get_type(), SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID);
    EXPECT_EQ(forwarded_subscribe->entry(0).get_event_group_id(), 0x0004);
}

TEST(ServiceManager, rejects_empty_external_sd_message) {
    auto router = std::make_shared<FakeServiceRouter>();
    auto endpoint = make_service_discovery_endpoint(*router);
    ServiceManager manager("service-test", "", router);

    manager.on_external_message(endpoint, MessageBuilder::create<SOMEIPSD>(), false);

    EXPECT_TRUE(router->captured_message.empty());
}

TEST(ServiceManager, rejects_external_subscribe_with_invalid_option_index) {
    auto router = std::make_shared<FakeServiceRouter>();
    auto endpoint = make_service_discovery_endpoint(*router);
    ServiceManager manager("service-test", "", router);
    auto subscribe = make_service_message(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID, 0x1001, 0x0002, 0x01, 0,
                                          SOMEIP_DEFAULT_TTL_ON, 0x00090001);
    subscribe->entry(0).set_event_group_id(0x0004);
    subscribe->entry(0).set_option1st_index(1);
    subscribe->entry(0).set_option1st_count(1);

    manager.on_external_message(endpoint, subscribe, false);

    ASSERT_FALSE(router->captured_message.empty());
    auto acknowledgement = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(MessageBuilder::build_message(*acknowledgement, router->captured_message.data(),
                                              static_cast<std::uint32_t>(router->captured_message.size())));
    ASSERT_EQ(acknowledgement->entries().size(), 1U);
    EXPECT_EQ(acknowledgement->entry(0).get_type(), SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    EXPECT_EQ(acknowledgement->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
}

TEST(ServiceManager, ignores_external_offer_without_endpoint_option) {
    auto router = std::make_shared<FakeServiceRouter>();
    auto endpoint = make_service_discovery_endpoint(*router);
    ServiceManager manager("service-test", "", router);
    auto offer = make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01, 0x00000003,
                                      SOMEIP_DEFAULT_TTL_ON, 0x00090001);

    manager.on_external_message(endpoint, offer, false);

    EXPECT_TRUE(router->captured_message.empty());
}

TEST(ServiceManager, external_subscribe_without_endpoint_option_sends_nack) {
    auto router = std::make_shared<FakeServiceRouter>();
    auto endpoint = make_service_discovery_endpoint(*router);
    ServiceManager manager("service-test", "", router);
    auto subscribe = make_service_message(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID, 0x1001, 0x0002, 0x01, 0,
                                          SOMEIP_DEFAULT_TTL_ON, 0x00090001);
    subscribe->entry(0).set_event_group_id(0x0004);

    manager.on_external_message(endpoint, subscribe, false);

    ASSERT_FALSE(router->captured_message.empty());
    auto acknowledgement = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(MessageBuilder::build_message(*acknowledgement, router->captured_message.data(),
                                              static_cast<std::uint32_t>(router->captured_message.size())));
    ASSERT_EQ(acknowledgement->entries().size(), 1U);
    EXPECT_EQ(acknowledgement->entry(0).get_type(), SOMEIP_SD_ENTRY::SUBSCRIBEACK::TYPEID);
    EXPECT_EQ(acknowledgement->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
}

TEST(ServiceManager, rejects_duplicate_external_endpoint_options) {
    auto router = std::make_shared<FakeServiceRouter>();
    auto endpoint = make_service_discovery_endpoint(*router);
    ServiceManager manager("service-test", "", router);
    auto subscribe = make_service_message(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID, 0x1001, 0x0002, 0x01, 0,
                                          SOMEIP_DEFAULT_TTL_ON, 0x00090001);
    subscribe->entry(0).set_event_group_id(0x0004);
    auto first = std::make_shared<IP4Address>();
    first->set_ip_address("192.0.2.1");
    first->set_port_address(40001);
    first->set_reliable(false);
    auto second = std::make_shared<IP4Address>();
    second->set_ip_address("192.0.2.1");
    second->set_port_address(40002);
    second->set_reliable(false);
    SDOption first_option(SOMEIP_SD_OPTION::IP4::TYPEID);
    first_option.set_address_option(first);
    SDOption second_option(SOMEIP_SD_OPTION::IP4::TYPEID);
    second_option.set_address_option(second);
    subscribe->options().push_back(first_option);
    subscribe->options().push_back(second_option);
    subscribe->entry(0).set_option1st_index(0);
    subscribe->entry(0).set_option1st_count(2);

    manager.on_external_message(endpoint, subscribe, false);

    ASSERT_FALSE(router->captured_message.empty());
    auto acknowledgement = MessageBuilder::create<SOMEIPSD>();
    ASSERT_TRUE(MessageBuilder::build_message(*acknowledgement, router->captured_message.data(),
                                              static_cast<std::uint32_t>(router->captured_message.size())));
    ASSERT_EQ(acknowledgement->entries().size(), 1U);
    EXPECT_EQ(acknowledgement->entry(0).get_ttl(), SOMEIP_DEFAULT_TTL_OFF);
}

TEST(ServiceManager, writes_current_state_for_configured_internal_provider) {
    ScopedServiceManagerConfig config_file;
    auto router = std::make_shared<FakeServiceRouter>();
    ServiceManager manager("service-test", config_file.path(), router);

    manager.on_internal_message(make_service_message(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID, 0x1001, 0x0002, 0x01,
                                                     0x00000003, SOMEIP_DEFAULT_TTL_ON, 0x00070001));
    ASSERT_TRUE(router->wait_for_add_routes(1));
    auto subscribe = make_service_message(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID, 0x1001, 0x0002, 0x01, 0,
                                          SOMEIP_DEFAULT_TTL_ON, 0x00050001);
    subscribe->entry(0).set_event_group_id(0x0004);
    manager.on_internal_message(subscribe);

    const std::string suffix = std::to_string(::getpid());
    const std::string provider_name = "provider-" + suffix;
    const std::string consumer_name = "consumer-" + suffix;
    const std::string provider_output_path = "/tmp/someip/dumpservices/" + provider_name + ".json";
    const std::string consumer_output_path = "/tmp/someip/dumpservices/" + consumer_name + ".json";
    ::mkdir("/tmp/someip", 0777);
    std::remove(provider_output_path.c_str());
    std::remove(consumer_output_path.c_str());
    EXPECT_TRUE(manager.write_current_state_someip_services({{0x0005, consumer_name}, {0x0007, provider_name}}));

    std::ifstream provider_output(provider_output_path);
    ASSERT_TRUE(provider_output.good());
    const std::string provider_contents((std::istreambuf_iterator<char>(provider_output)),
                                        std::istreambuf_iterator<char>());
    EXPECT_NE(provider_contents.find("Provided_services"), std::string::npos);
    EXPECT_NE(provider_contents.find("0x1001"), std::string::npos);
    provider_output.close();

    std::ifstream consumer_output(consumer_output_path);
    ASSERT_TRUE(consumer_output.good());
    const std::string consumer_contents((std::istreambuf_iterator<char>(consumer_output)),
                                        std::istreambuf_iterator<char>());
    EXPECT_NE(consumer_contents.find("Required_services"), std::string::npos);
    EXPECT_NE(consumer_contents.find("0x0004"), std::string::npos);
    consumer_output.close();
    std::remove(provider_output_path.c_str());
    std::remove(consumer_output_path.c_str());
}
