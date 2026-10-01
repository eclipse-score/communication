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

#include <arpa/inet.h>
#include <cstdio>
#include <exception/ConfigurationError.h>
#include <config/Configuration.h>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

char myjson[] = R"(
{
    "unicast" : "fe80::34f7:9e20:36ae:aff4",
    "iptype" : "ipv6",
    "logging" :
    {
        "level" : "info",
        "console" : "true",
        "file" : { "enable" : "true", "path" : "/var/log/vsomeip.log" },
        "dlt" : "true"
    },
    "applications" :
    [
        {
            "name" : "client-sample",
            "id" : "0x1343"
        },
        {
            "name" : "second-client-sample",
            "id" : "0x1344"
        },
        {
            "name" : "third-client-sample",
            "id" : "0x1345"
        },
        {
            "name" : "fourth-client-sample",
            "id" : "0x1346"
        }
    ],
    "services" :
    [
        {
            "service" : "0x1234",
            "instance" : "0x5678",
            "unicast" : "192.168.56.102",
            "reliable" : { "port" : "30509", "magic-cookies" : false },
            "events" :
            [
                {
                    "event" : "0x0777",
                    "is_field" : "true"
                },
                {
                    "event" : "0x0778",
                    "is_field" : "false"
                },
                {
                    "event" : "0x0779",
                    "is_field" : "true"
                }
            ],
            "eventgroups" :
            [
                {
                    "eventgroup" : "0x4455",
                    "events" : [ "0x777", "0x778" ]
                },
                {
                    "eventgroup" : "0x4465",
                    "events" : [ "0x778", "0x779" ]
                },
                {
                    "eventgroup" : "0x4555",
                    "events" : [ "0x777", "0x779" ]
                }
            ]
        }
    ],
    "routing" : "client-sample",
    "service-discovery" :
    {
        "enable" : "true",
        "multicast" : "FF02::1:FF00",
        "port" : "30491",
        "protocol" : "udp",
        "initial_delay_min" : "10",
        "initial_delay_max" : "100",
        "repetitions_base_delay" : "200",
        "repetitions_max" : "3",
        "ttl" : "3",
        "cyclic_offer_delay" : "2000",
        "request_response_delay" : "1500"
    }
}
)";

class ScopedConfigFile {
public:
    ScopedConfigFile(const char* path, const char* contents) : path_(path) {
        std::ofstream file(path_);
        if (!file) {
            throw std::runtime_error("Unable to create test configuration file: " + path_);
        }
        file << contents;
    }

    ~ScopedConfigFile() {
        std::remove(path_.c_str());
    }

private:
    std::string path_;
};

TEST(ConfigurationTest, Configuration) {
    ScopedConfigFile config_file("ex.json", myjson);

    Configuration config("ex.json");
    EXPECT_EQ(config.get_application_id("client-sample"), 0x1343);
    EXPECT_EQ(config.get_ip_type(), 6);
}

TEST(ConfigurationTest, UsesEnvironmentConfigurationWhenPathIsEmpty) {
    ScopedConfigFile config_file("env.json", myjson);
    ASSERT_EQ(setenv("LGSOMEIP_CONFIGURATION", "env.json", 1), 0);

    Configuration config;

    EXPECT_EQ(config.get_application_id("client-sample"), 0x1343);
    EXPECT_EQ(unsetenv("LGSOMEIP_CONFIGURATION"), 0);
}

TEST(ConfigurationTest, RejectsDuplicateApplicationIds) {
    ScopedConfigFile config_file("duplicate_application_ids.json",
                                 R"({
            "unicast": "127.0.0.1",
            "applications": [
                { "name": "first", "id": "0x0100" },
                { "name": "second", "id": "0x0100" }
            ],
            "service-discovery": {
                "enable": "true",
                "multicast": "224.244.224.245",
                "port": "30490"
            }
        })");

    EXPECT_THROW({ Configuration config("duplicate_application_ids.json"); }, ConfigurationErrorException);
}

TEST(ConfigurationTest, RejectsMalformedJson) {
    ScopedConfigFile config_file("malformed.json", "[");

    EXPECT_THROW({ Configuration config("malformed.json"); }, ConfigurationErrorException);
}

TEST(ConfigurationTest, RejectsInvalidApplicationId) {
    ScopedConfigFile config_file("invalid_application_id.json",
                                 R"({
            "unicast": "127.0.0.1",
            "applications": [
                { "name": "first", "id": "not-a-number" }
            ],
            "service-discovery": {
                "enable": "true",
                "multicast": "224.244.224.245",
                "port": "30490"
            }
        })");

    EXPECT_THROW({ Configuration config("invalid_application_id.json"); }, ConfigurationErrorException);
}

TEST(ConfigurationTest, RejectsInvalidNestedServiceShape) {
    ScopedConfigFile config_file("invalid_service_shape.json",
                                 R"({
            "unicast": "127.0.0.1",
            "applications": [],
            "services": [
                {
                    "service": "0x1001",
                    "instance": "0x0001",
                    "events": { "event": "0x0001" }
                }
            ],
            "service-discovery": {
                "enable": "true",
                "multicast": "224.244.224.245",
                "port": "30490"
            }
        })");

    EXPECT_THROW({ Configuration config("invalid_service_shape.json"); }, ConfigurationErrorException);
}

TEST(ConfigurationTest, ServiceInfo) {
    ScopedConfigFile config_file("ex.json", myjson);
    Configuration config("ex.json");

    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_service_id(), 0x1234);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_instance_id(), 0x5678);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_reliable_port(), 30509);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_state_magic_cookies(), false);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_event(0x0777)->get_event_id(), 0x777);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_event_group(0x4455)->back(), 0x778);
    EXPECT_EQ((*(config.get_service_info(0x1234, 0x5678)->get_events()))[0x0777][0], 0x4455);
    EXPECT_EQ((*(config.get_service_info(0x1234, 0x5678)->get_events()))[0x0777][1], 0x4555);
}

TEST(ConfigurationTest, ServiceDiscovery) {
    ScopedConfigFile config_file("ex.json", myjson);
    Configuration config("ex.json");

    EXPECT_EQ(config.get_service_discovery_info()->is_enabled(), true);
    EXPECT_EQ(config.get_service_discovery_info()->get_multicast(), "FF02::1:FF00");
    EXPECT_EQ(config.get_service_discovery_info()->get_port(), 30491);
    EXPECT_EQ(config.get_service_discovery_info()->get_initial_delay_min(), 10);
    EXPECT_EQ(config.get_service_discovery_info()->get_initial_delay_max(), 100);
    EXPECT_EQ(config.get_service_discovery_info()->get_repetitions_base_delay(), 200);
    EXPECT_EQ(config.get_service_discovery_info()->get_repetitions_max(), 3);
    EXPECT_EQ(config.get_service_discovery_info()->get_ttl(), 3);
    EXPECT_EQ(config.get_service_discovery_info()->get_cyclic_offer_delay(), 2000);
    EXPECT_EQ(config.get_service_discovery_info()->get_request_response_delay(), 1500);
}

TEST(ConfigurationTest, Event) {
    ScopedConfigFile config_file("ex.json", myjson);
    Configuration config("ex.json");
    ConfigEvent* event = config.get_service_info(0x1234, 0x5678)->get_event(0x0777);

    EXPECT_EQ(event->get_event_id(), 0x777);
    EXPECT_EQ(event->is_field(), true);
    EXPECT_EQ(event->get_update_cycle(), 0);
}

char myjson2[] = R"(
{
    "unicast" : "192.168.56.101",
    "logging" :
    {
        "level" : "debug",
        "console" : "true",
        "file" : { "enable" : "false", "path" : "/tmp/vsomeip.log" },
        "dlt" : "false"
    },
    "applications" :
    [
        {
            "name" : "response",
            "id" : "0x01"
        },
        {
            "name" : "request-1",
            "id" : "0x11"
        },
        {
            "name" : "request-2",
            "id" : "0x12"
        }
    ],
    "services" :
    [
        {
            "service" : "0x1234",
            "instance" : "0x5678",
            "reliable" : { "port" : "30509", "enable-magic-cookies" : "false" },
            "events" :
            [
                {
                    "event" : "0x8777",
                    "is_field" : "false",
                    "is_reliable" : "true",
                    "update-cycle" : "2000"
                },
                {
                    "event" : "0x8778",
                    "is_field" : "true",
                    "is_reliable" : "true",
                    "update-cycle" : 0
                },
                {
                    "event" : "0x8779",
                    "is_field" : "false",
                    "is_reliable" : "true"
                }
            ],
            "eventgroups" :
            [
                {
                    "eventgroup" : "0x4455",
                    "events" : [ "0x8777", "0x8778" ]
                },
                {
                    "eventgroup" : "0x4465",
                    "events" : [ "0x8778", "0x8779" ]
                },
                {
                    "eventgroup" : "0x4555",
                    "events" : [ "0x8777", "0x8779" ]
                }
            ]
        },
        {
            "service" : "0x1235",
            "instance" : "0x5678",
            "unreliable" : "30509",
            "multicast" :
            {
                "address" : "224.225.226.234",
                "port" : "32344"
            }
        }
    ],
    "routing" : "service-sample",
    "service-discovery" :
    {
        "enable" : "true",
        "multicast" : "224.244.224.245",
        "port" : "30490",
        "protocol" : "udp",
        "initial_delay_min" : "10",
        "initial_delay_max" : "100",
        "repetitions_base_delay" : "200",
        "repetitions_max" : "3",
        "ttl" : "3",
        "cyclic_offer_delay" : "2000",
        "request_response_delay" : "1500"
    }
}
)";

TEST(ConfigurationTest, Configuration2) {
    ScopedConfigFile config_file("ex2.json", myjson2);

    Configuration config("ex2.json");
    EXPECT_EQ(config.get_application_id("response"), 0x01);
    EXPECT_EQ(config.get_ip_type(), 4);
}

TEST(ConfigurationTest, ServiceInfo2) {
    ScopedConfigFile config_file("ex2.json", myjson2);
    Configuration config("ex2.json");

    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_service_id(), 0x1234);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_instance_id(), 0x5678);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_reliable_port(), 30509);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_state_magic_cookies(), false);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_event(0x8777)->get_event_id(), 0x8777);
    EXPECT_EQ(config.get_service_info(0x1234, 0x5678)->get_event_group(0x4455)->back(), 0x8778);
    EXPECT_EQ((*(config.get_service_info(0x1234, 0x5678)->get_events()))[0x8777][0], 0x4455);
    EXPECT_EQ((*(config.get_service_info(0x1234, 0x5678)->get_events()))[0x8777][1], 0x4555);

    EXPECT_EQ(config.get_service_info(0x1235, 0x5678)->get_service_id(), 0x1235);
    EXPECT_EQ(config.get_service_info(0x1235, 0x5678)->get_instance_id(), 0x5678);
}

TEST(ConfigurationTest, ServiceDiscovery2) {
    ScopedConfigFile config_file("ex2.json", myjson2);
    Configuration config("ex2.json");

    EXPECT_EQ(config.get_service_discovery_info()->is_enabled(), true);
    EXPECT_EQ(config.get_service_discovery_info()->get_multicast(), "224.244.224.245");
    EXPECT_EQ(config.get_service_discovery_info()->get_port(), 30490);
    EXPECT_EQ(config.get_service_discovery_info()->get_initial_delay_min(), 10);
    EXPECT_EQ(config.get_service_discovery_info()->get_initial_delay_max(), 100);
    EXPECT_EQ(config.get_service_discovery_info()->get_repetitions_base_delay(), 200);
    EXPECT_EQ(config.get_service_discovery_info()->get_repetitions_max(), 3);
    EXPECT_EQ(config.get_service_discovery_info()->get_ttl(), 3);
    EXPECT_EQ(config.get_service_discovery_info()->get_cyclic_offer_delay(), 2000);
    EXPECT_EQ(config.get_service_discovery_info()->get_request_response_delay(), 1500);
}

TEST(ConfigurationTest, Event2) {
    ScopedConfigFile config_file("ex2.json", myjson2);
    Configuration config("ex2.json");
    ConfigEvent* event = config.get_service_info(0x1234, 0x5678)->get_event(0x8777);

    EXPECT_EQ(event->get_event_id(), 0x8777);
    EXPECT_EQ(event->is_field(), false);
    EXPECT_EQ(event->get_update_cycle(), 2000);
}
