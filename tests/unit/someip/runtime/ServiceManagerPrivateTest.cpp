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

#include <config/Configuration.h>
#include <message/MessageComposer.h>
#include <packetrouter/ServiceRouter.h>
#include <runtime/ApplicationCommonTypes.h>
#include <runtime/ApplicationConstant.h>
#include <utils/time/TimerMux.h>
#include <unistd.h>

#define private public
#include <runtime/ServiceManager.h>
#undef private

#include <cstdio>
#include <fstream>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

class PrivateServiceRouter final : public ServiceRouter {
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

class ScopedPrivateConfig final {
public:
    ScopedPrivateConfig() : path_("/tmp/lgsomeip-service-manager-private-" + std::to_string(::getpid()) + ".json") {
        std::ofstream file(path_);
        file << R"({
            "unicast": "127.0.0.1",
            "applications": [],
            "services": [
                {
                    "service": "0x1001", "instance": "0x0002", "unreliable": "30509", "is-provider": "true",
                    "events": [ { "event": "0x0003", "is_field": "false" } ],
                    "eventgroups": [ { "eventgroup": "0x0004", "events": [ "0x0003" ] } ]
                },
                { "service": "0x1002", "instance": "0x0003", "unreliable": "30510", "is-provider": "false" }
            ],
            "service-discovery": {
                "enable": "true", "multicast": "224.244.224.245", "port": "30490", "protocol": "udp",
                "initial_delay_min": "10", "initial_delay_max": "100", "repetitions_base_delay": "200",
                "repetitions_max": "3", "ttl": "3", "cyclic_offer_delay": "2000", "request_response_delay": "1500"
            }
        })";
    }

    ~ScopedPrivateConfig() {
        std::remove(path_.c_str());
    }

    const std::string& path() const {
        return path_;
    }

private:
    std::string path_;
};

TEST(ServiceManagerPrivate, matches_exact_any_and_minimum_versions) {
    auto router = std::make_shared<PrivateServiceRouter>();
    ServiceManager manager("private-test", "", router);

    EXPECT_TRUE(manager.check_service_version(1, 1, 3, 3, 0, false));
    EXPECT_TRUE(manager.check_service_version(SOMEIP_DEFAULT_ANY_MAJOR, 2, 99, SOMEIP_DEFAULT_ANY_MINOR, 0, false));
    EXPECT_TRUE(manager.check_service_version(1, 1, 10, 12, 10, true));
    EXPECT_FALSE(manager.check_service_version(1, 2, 10, 12, 10, true));
    EXPECT_FALSE(manager.check_service_version(1, 1, 3, 4, 0, false));
}

TEST(ServiceManagerPrivate, rolls_session_id_and_clears_reboot_flag) {
    auto router = std::make_shared<PrivateServiceRouter>();
    ServiceManager manager("private-test", "", router);
    manager.session_info_["192.0.2.1"] = {0xffff, 0x80};

    auto session = manager.get_session_id_and_reboot_flag("192.0.2.1");

    EXPECT_EQ(session.first, 1);
    EXPECT_EQ(session.second, 0);
}

TEST(ServiceManagerPrivate, parses_endpoint_options_and_rejects_duplicates) {
    auto router = std::make_shared<PrivateServiceRouter>();
    ServiceManager manager("private-test", "", router);
    auto tcp = std::make_shared<IP4Address>();
    tcp->set_ip_address("192.0.2.10");
    tcp->set_port_address(30509);
    tcp->set_reliable(true);
    auto udp = std::make_shared<IP4Address>();
    udp->set_ip_address("192.0.2.11");
    udp->set_port_address(30510);
    udp->set_reliable(false);
    SDOption options[2] = {SDOption(SOMEIP_SD_OPTION::IP4::TYPEID), SDOption(SOMEIP_SD_OPTION::IP4::TYPEID)};
    options[0].set_address_option(tcp);
    options[1].set_address_option(udp);

    std::shared_ptr<Address> parsed_tcp;
    std::shared_ptr<Address> parsed_udp;
    EXPECT_TRUE(manager.find_option_address(parsed_tcp, parsed_udp, options, 2, nullptr, 0));
    ASSERT_NE(parsed_tcp, nullptr);
    ASSERT_NE(parsed_udp, nullptr);
    EXPECT_EQ(parsed_tcp->get_ip_address(), tcp->get_ip_address());
    EXPECT_EQ(parsed_tcp->get_port_address(), tcp->get_port_address());
    EXPECT_EQ(parsed_udp->get_ip_address(), udp->get_ip_address());
    EXPECT_EQ(parsed_udp->get_port_address(), udp->get_port_address());

    options[1].set_address_option(tcp);
    EXPECT_FALSE(manager.find_option_address(parsed_tcp, parsed_udp, options, 2, nullptr, 0));
}

TEST(ServiceManagerPrivate, advances_repetition_offer_and_find_phases) {
    ScopedPrivateConfig config_file;
    auto router = std::make_shared<PrivateServiceRouter>();
    ServiceManager manager("private-test", config_file.path(), router);
    auto offer_message = MessageBuilder::create<SOMEIPSD>();
    auto find_message = MessageBuilder::create<SOMEIPSD>();

    AvailableService offered(0x0007, true, 0x01, 0x00000003, 3, nullptr, nullptr);
    offered.state = SOMEIP_SERVICE_STATE_INITIAL;
    offered.timer = 1;
    manager.repetition_offer_list_[0x1001][0x0002] = offered;
    manager.on_send_cyclic_repetition_offer_service(offer_message);
    offer_message->entries().clear();
    manager.repetition_offer_list_[0x1001][0x0002].timer = 1;
    manager.on_send_cyclic_repetition_offer_service(offer_message);
    ASSERT_EQ(offer_message->entries().size(), 1U);
    offer_message->entries().clear();
    manager.repetition_offer_list_[0x1001][0x0002].state =
        SOMEIP_SERVICE_STATE_REPETITION |
        manager.get_configuration()->get_service_discovery_info()->get_repetitions_max();
    manager.repetition_offer_list_[0x1001][0x0002].timer = 1;
    manager.on_send_cyclic_repetition_offer_service(offer_message);
    for (int i = 0; i < 11; ++i) {
        manager.on_send_cyclic_repetition_offer_service(offer_message);
    }
    EXPECT_TRUE(manager.repetition_offer_list_.empty());

    RequestedService requested(0x0005, 0x01, 0x00000003);
    requested.state = SOMEIP_SERVICE_STATE_INITIAL;
    requested.timer = 1;
    manager.repetition_find_list_[0x1002][0x0003].push_back(requested);
    manager.on_send_cyclic_repetition_find_service(find_message);
    find_message->entries().clear();
    manager.repetition_find_list_[0x1002][0x0003].front().timer = 1;
    manager.on_send_cyclic_repetition_find_service(find_message);
    ASSERT_EQ(find_message->entries().size(), 1U);
    find_message->entries().clear();
    manager.repetition_find_list_[0x1002][0x0003].front().state =
        SOMEIP_SERVICE_STATE_REPETITION |
        manager.get_configuration()->get_service_discovery_info()->get_repetitions_max();
    manager.repetition_find_list_[0x1002][0x0003].front().timer = 1;
    manager.on_send_cyclic_repetition_find_service(find_message);
    EXPECT_TRUE(manager.repetition_find_list_.empty());

    EXPECT_TRUE(offer_message->entries().empty());
    EXPECT_TRUE(find_message->entries().empty());
}

TEST(ServiceManagerPrivate, removes_expired_service_after_ttl_timer) {
    auto router = std::make_shared<PrivateServiceRouter>();
    ServiceManager manager("private-test", "", router);
    manager.available_service_list_[0x1001][0x0002] = AvailableService(0, false, 1, 3, 0, nullptr, nullptr);

    for (int i = 0; i < 11; ++i) {
        manager.on_check_ttl_service();
    }

    ASSERT_EQ(manager.available_service_list_.count(0x1001), 1U);
    EXPECT_TRUE(manager.available_service_list_[0x1001].empty());
}

TEST(ServiceManagerPrivate, sends_cyclic_main_offer_for_main_provider) {
    ScopedPrivateConfig config_file;
    auto router = std::make_shared<PrivateServiceRouter>();
    ServiceManager manager("private-test", config_file.path(), router);
    auto message = MessageBuilder::create<SOMEIPSD>();
    auto& service = manager.available_service_list_[0x1001][0x0002];
    service = AvailableService(0x0007, true, 0x01, 0x00000003, 3, nullptr, nullptr);
    service.state = SOMEIP_SERVICE_STATE_MAIN;

    for (int i = 0; i < 25 && message->entries().empty(); ++i) {
        manager.on_send_cyclic_main_offer_service(message);
    }

    ASSERT_EQ(message->entries().size(), 1U);
    EXPECT_EQ(message->entry(0).get_type(), SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
}

TEST(ServiceManagerPrivate, removes_expired_subscription_after_ttl_timer) {
    ScopedPrivateConfig config_file;
    auto router = std::make_shared<PrivateServiceRouter>();
    ServiceManager manager("private-test", config_file.path(), router);
    auto& service = manager.available_service_list_[0x1001][0x0002];
    service = AvailableService(0x0007, true, 0x01, 0x00000003, 3, nullptr, nullptr);
    RequestedSubscribe subscription;
    subscription.app_id = 0x0005;
    subscription.ttl = 0;
    service.subscribe[0x0004].push_back(subscription);

    for (int i = 0; i < 11; ++i) {
        manager.on_check_ttl_subscribe();
    }

    EXPECT_TRUE(service.subscribe[0x0004].empty());
}
