/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#include "gtest/gtest.h"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/service_interface_identifier.hpp"

using namespace ::testing;

namespace score::socom {

class ServiceInterfaceIdentifierTest : public Test {
   protected:
    static constexpr std::string_view service_interface_id{"service1"};
    static constexpr std::string_view service_interface_id_2{"service2"};
    Service_interface_identifier::Version const service_interface_version{1, 0};
    Service_interface_identifier const service_interface{service_interface_id,
                                                         service_interface_version};
    Service_interface_identifier const service_interface_2{service_interface_id_2,
                                                           service_interface_version};
    std::size_t num_methods{1U};
    std::size_t num_methods_2{2U};
    std::size_t num_events{2U};
    std::size_t num_events_2{3U};

    Service_interface_definition const interface_config_1{
        service_interface, to_num_of_methods(num_methods), to_num_of_events(num_events)};
    Service_interface_definition const interface_config_2{
        service_interface_2, to_num_of_methods(num_methods), to_num_of_events(num_events)};
    Service_interface_definition const interface_config_3{
        service_interface, to_num_of_methods(num_methods_2), to_num_of_events(num_events)};
    Service_interface_definition const interface_config_4{
        service_interface, to_num_of_methods(num_methods), to_num_of_events(num_events_2)};
};

using ServiceInterfaceConfigurationDeathTest = ServiceInterfaceIdentifierTest;

constexpr std::string_view ServiceInterfaceIdentifierTest::service_interface_id;
constexpr std::string_view ServiceInterfaceIdentifierTest::service_interface_id_2;

TEST_F(ServiceInterfaceIdentifierTest, ConfigurationEqual) {
    ASSERT_TRUE(interface_config_1 == interface_config_1);
    ASSERT_FALSE(interface_config_1 == interface_config_2);
}

TEST_F(ServiceInterfaceIdentifierTest, LiteratorConstructorUsesStringView) {
    String_registry registry;
    auto const registry_string_view = registry.insert(service_interface_id).first;

    auto const interface =
        Service_interface_identifier{registry_string_view, service_interface_version};
    EXPECT_EQ(interface.id, registry_string_view);
    EXPECT_EQ(interface.id.data(), registry_string_view.data());
}

TEST_F(ServiceInterfaceIdentifierTest, StringConstructorUsesStringView) {
    auto const interface =
        Service_interface_identifier{std::string(service_interface_id), service_interface_version};
    EXPECT_EQ(interface.id.string_view(), service_interface_id);

    auto const registry_string_view = service_id_registry().insert(service_interface_id).first;
    EXPECT_EQ(interface.id, registry_string_view);
    EXPECT_EQ(interface.id.data(), registry_string_view.data());
}

TEST_F(ServiceInterfaceIdentifierTest, StringViewConstructorUsesStringView) {
    auto const interface =
        Service_interface_identifier{service_interface_id, service_interface_version};
    EXPECT_EQ(interface.id.string_view(), service_interface_id);

    auto const registry_string_view = service_id_registry().insert(service_interface_id).first;
    EXPECT_EQ(interface.id, registry_string_view);
    EXPECT_EQ(interface.id.data(), registry_string_view.data());
}

TEST_F(ServiceInterfaceIdentifierTest, StringViewLiteralConstructorUsesStringView) {
    auto const interface = Service_interface_identifier{service_interface_id, Literal_tag{},
                                                        service_interface_version};
    EXPECT_EQ(interface.id.string_view(), service_interface_id);

    auto const registry_string_view = service_id_registry().insert(service_interface_id).first;
    EXPECT_EQ(interface.id, registry_string_view);
    EXPECT_EQ(interface.id.data(), registry_string_view.data());
}

TEST_F(ServiceInterfaceConfigurationDeathTest, SameInterfaceDifferentNumMethodsAsserts) {
    EXPECT_DEATH((void)(interface_config_1 == interface_config_3), "[Aa]ssertion.*failed");
}

TEST_F(ServiceInterfaceConfigurationDeathTest, SameInterfaceDifferentNumEventsAsserts) {
    EXPECT_DEATH((void)(interface_config_1 == interface_config_4), "[Aa]ssertion.*failed");
}

class ServiceInstanceTest : public Test {
   protected:
    static constexpr std::string_view id_string{"idd"};
};

constexpr std::string_view ServiceInstanceTest::id_string;

TEST_F(ServiceInstanceTest, LiteratorConstructorUsesStringView) {
    String_registry registry;
    auto const registry_string_view = registry.insert(id_string).first;

    auto const instance = Service_instance{registry_string_view};
    EXPECT_EQ(instance.id, registry_string_view);
    EXPECT_EQ(instance.id.data(), registry_string_view.data());
}

TEST_F(ServiceInstanceTest, StringConstructorAddsStringToRegistry) {
    auto const instance = Service_instance{std::string(id_string)};
    EXPECT_EQ(instance.id.string_view(), id_string);

    auto const registry_string_view = instance_id_registry().insert(id_string).first;
    EXPECT_EQ(instance.id, registry_string_view);
    EXPECT_EQ(instance.id.data(), registry_string_view.data());
}

TEST_F(ServiceInstanceTest, StringViewConstructorAddsStringToRegistry) {
    auto const instance = Service_instance{id_string};
    EXPECT_EQ(instance.id.string_view(), id_string);

    auto const registry_string_view = instance_id_registry().insert(id_string).first;
    EXPECT_EQ(instance.id, registry_string_view);
    EXPECT_EQ(instance.id.data(), registry_string_view.data());
}

TEST_F(ServiceInstanceTest, StringViewLiteralConstructorAddsStringToRegistry) {
    auto const instance = Service_instance{id_string, Literal_tag{}};
    EXPECT_EQ(instance.id.string_view(), id_string);

    auto const registry_string_view = instance_id_registry().insert(id_string, Literal_tag{}).first;
    EXPECT_EQ(instance.id, registry_string_view);
    EXPECT_EQ(instance.id.data(), registry_string_view.data());
}

}  // namespace score::socom
