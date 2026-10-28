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
#include <string>

#include <config/ServiceInfo.h>

using namespace lgsomeip;

TEST(ServiceInfo, parses_optional_transport_and_event_metadata) {
    rapidjson::Document document;
    document.Parse(R"({
        "service": "0x1234",
        "name": "sample",
        "instance": "0x0001",
        "major_version": "0x02",
        "minor_version": "0x00000003",
        "minimum_minor_version": "0x00000002",
        "reliable": { "port": "30509", "enable-magic-cookies": true },
        "unreliable": "30510",
        "someiptp": ["0x1001", "0x1002"],
        "multicast": { "address": "224.0.0.1", "port": "30490" },
        "is-provider": true,
        "secure-connection": true,
        "events": [
            { "event": "0x1001", "is_field": true, "update-cycle": 100 }
        ],
        "eventgroups": [
            {
                "eventgroup": "0x4455",
                "is_multicast": true,
                "threshold": 2,
                "multicast": { "address": "224.0.0.2", "port": "30491" },
                "events": ["0x1001"]
            }
        ]
    })");

    std::string ip_address = "127.0.0.1";
    ServiceInfo service;
    service.initialize(document, ip_address, 4);

    EXPECT_EQ(service.get_service_id(), 0x1234);
    EXPECT_EQ(service.get_service_name(), "sample");
    EXPECT_EQ(service.get_instance_id(), 0x0001);
    EXPECT_TRUE(service.has_instance_id());
    EXPECT_EQ(service.get_major_version(), 0x02);
    EXPECT_TRUE(service.has_major_version());
    EXPECT_EQ(service.get_minor_version(), 0x00000003U);
    EXPECT_TRUE(service.has_minor_version());
    EXPECT_EQ(service.get_minimum_minor_version(), 0x00000002U);
    EXPECT_TRUE(service.has_minimum_minor_version());

    ASSERT_NE(service.get_reliable_address(), nullptr);
    EXPECT_EQ(service.get_reliable_port(), 30509);
    EXPECT_TRUE(service.get_state_magic_cookies());
    ASSERT_NE(service.get_unreliable_address(), nullptr);
    EXPECT_EQ(service.get_unreliable_port(), 30510);

    ASSERT_NE(service.get_multicast_address(), nullptr);
    ASSERT_EQ(service.get_multicast_address()->size(), 2U);
    EXPECT_EQ(service.get_multicast_address()->at(0)->get_ip_address(), "224.0.0.1");
    EXPECT_EQ(service.get_multicast_address()->at(1)->get_ip_address(), "224.0.0.2");
    EXPECT_TRUE(service.is_provider());
    EXPECT_TRUE(service.is_secure_connection());

    EXPECT_EQ(service.get_tp_list(), (std::vector<std::uint16_t>{0x1001, 0x1002}));
    ASSERT_NE(service.get_event(0x1001), nullptr);
    EXPECT_TRUE(service.get_event(0x1001)->is_field());
    ASSERT_NE(service.get_event_group(0x4455), nullptr);
    EXPECT_EQ(*service.get_event_group(0x4455), (std::vector<std::uint16_t>{0x1001}));

    auto* event_group = service.get_event_group_object(0x4455);
    ASSERT_NE(event_group, nullptr);
    EXPECT_EQ(event_group->get_threshold(), 2);
    EXPECT_TRUE(event_group->is_multicast());
    ASSERT_NE(event_group->get_multicast_address(), nullptr);
    EXPECT_EQ(event_group->get_multicast_address()->get_ip_address(), "224.0.0.2");
}
