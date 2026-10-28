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
#include <rapidjson/document.h>

#include <config/ConfigurationSD.h>

using namespace lgsomeip;

TEST(ConfigurationSD, initialize_sets_defaults) {
    ConfigurationSD configuration;
    configuration.initialize();

    EXPECT_TRUE(configuration.is_enabled());
    EXPECT_EQ(configuration.get_multicast(), "224.244.224.245");
    EXPECT_EQ(configuration.get_protocol(), "udp");
    EXPECT_EQ(configuration.get_port(), 30490);
    EXPECT_EQ(configuration.get_repetitions_max(), 3);
    EXPECT_EQ(configuration.get_ttl(), 3);
}

TEST(ConfigurationSD, parses_numeric_values) {
    rapidjson::Document document;
    document.Parse(R"({
        "enable": true,
        "multicast": "224.0.0.1",
        "protocol": "udp",
        "port": 30490,
        "vlan_qos": 7,
        "initial_delay_min": 10,
        "initial_delay_max": 20,
        "repetitions_base_delay": 30,
        "repetitions_max": 4,
        "ttl": 5,
        "cyclic_offer_delay": 40,
        "request_response_delay": 50
    })");

    ConfigurationSD configuration;
    configuration.initialize(document);

    EXPECT_TRUE(configuration.is_enabled());
    EXPECT_EQ(configuration.get_multicast(), "224.0.0.1");
    EXPECT_EQ(configuration.get_port(), 30490);
    EXPECT_EQ(configuration.get_vlan_priority(), 7);
    EXPECT_EQ(configuration.get_initial_delay_min(), 10);
    EXPECT_EQ(configuration.get_initial_delay_max(), 20);
    EXPECT_EQ(configuration.get_repetitions_base_delay(), 30);
    EXPECT_EQ(configuration.get_repetitions_max(), 4);
    EXPECT_EQ(configuration.get_ttl(), 5);
    EXPECT_EQ(configuration.get_cyclic_offer_delay(), 40);
    EXPECT_EQ(configuration.get_request_response_delay(), 50);
}

TEST(ConfigurationSD, disabled_configuration_stops_after_enable_flag) {
    rapidjson::Document document;
    document.Parse(R"({
        "enable": false,
        "multicast": "224.0.0.1",
        "port": 30490
    })");

    ConfigurationSD configuration;
    configuration.initialize(document);

    EXPECT_FALSE(configuration.is_enabled());
    EXPECT_EQ(configuration.get_port(), 0);
}
