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

#include <limits>
#include <sstream>
#include <string_view>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "score/socom/clients_t.hpp"
#include "score/socom/connector_factory.hpp"
#include "score/socom/server_t.hpp"
#include "score/socom/service_interface_identifier.hpp"
#include "score/socom/utilities.hpp"

using ::testing::TestParamInfo;
using ::testing::Values;

namespace score::socom {

enum class Expectation { connect, no_connect };

std::ostream& operator<<(std::ostream& out, Expectation const& e) {
    if (Expectation::connect == e) {
        out << "connect";
    } else {
        out << "no_connect";
    }
    return out;
}

using Check_log = std::function<void()>;

using Interface_configuration_and_expected_connection =
    std::tuple<Server_service_interface_definition, Server_service_interface_definition,
               Expectation>;

class InterfaceCompatibilityTest
    : public ::testing::TestWithParam<Interface_configuration_and_expected_connection> {
   protected:
    Service_instance const instance{"VersionCompatibilityTest", Literal_tag{}};
};

Check_log expect_logs(Expectation const expected_connection,
                      Server_service_interface_definition const& client_conf,
                      Server_service_interface_definition const& server_conf) {
    if ((Expectation::no_connect == expected_connection) &&
        (client_conf.get_interface().version.major == server_conf.get_interface().version.major) &&
        (client_conf.get_interface().id == server_conf.get_interface().id)) {
        // start capturing stdout
        testing::internal::CaptureStdout();
        // return lambda that encapsulates the reading of stdout and verification
        return []() {
            auto const expected_message =
                "SOCom error: Bind client to server - minor version incompatible";
            auto const output_cout = testing::internal::GetCapturedStdout();
            ASSERT_THAT(output_cout, ::testing::HasSubstr(expected_message));
        };
    }
    return []() {};
}

TEST_P(InterfaceCompatibilityTest, ServerCreatedBeforeClient) {
    auto const& client_conf = std::get<0>(GetParam());
    auto const& server_conf = std::get<1>(GetParam());
    auto const& expected_connection = std::get<2>(GetParam());

    auto log_expectation = expect_logs(expected_connection, client_conf, server_conf);

    Connector_factory factory{client_conf, instance};
    Server_data server{factory, server_conf, instance};
    if (Expectation::connect == expected_connection) {
        Client_data client1{factory};
    } else {
        Client_data client1{factory, Client_data::no_connect};
    }

    log_expectation();
}

TEST_P(InterfaceCompatibilityTest, ClientCreatedBeforeServer) {
    auto const& client_conf = std::get<0>(GetParam());
    auto const& server_conf = std::get<1>(GetParam());
    auto const& expected_connection = std::get<2>(GetParam());

    auto log_expectation = expect_logs(expected_connection, client_conf, server_conf);

    Connector_factory factory{client_conf, instance};
    Client_data client{factory, Client_data::no_connect};

    std::atomic<bool> const* not_available = nullptr;
    std::atomic<bool> const* available = nullptr;

    if (Expectation::connect == expected_connection) {
        not_available = &client.expect_service_state_change(Service_state::not_available);
        available = &client.expect_service_state_change(Service_state::available);
    }

    {
        Server_data server{factory, server_conf, instance};
        if (Expectation::connect == expected_connection) {
            ASSERT_NE(nullptr, available);
            wait_for_atomics(*available);
        }
    }

    if (Expectation::connect == expected_connection) {
        ASSERT_NE(nullptr, not_available);
        wait_for_atomics(*not_available);
    }

    log_expectation();
}

Interface_configuration_and_expected_connection create_conf(
    Service_interface_identifier const& client, Service_interface_identifier const& server,
    Expectation const& expected_connection) {
    using Server_conf = Server_service_interface_definition;
    return std::make_tuple(Server_conf{client, {}, {}}, Server_conf{server, {}, {}},
                           expected_connection);
}

constexpr std::string_view id0{"0"};
constexpr std::string_view id1{"1"};
constexpr std::string_view empty_id{""};
constexpr std::string_view huge_id{"biggest interface id on earth with huge text description"};

using Major_t = decltype(Service_interface_identifier::Version::major);
using Minor_t = decltype(Service_interface_identifier::Version::minor);
auto const min_major = std::numeric_limits<Major_t>::min();
auto const max_major = std::numeric_limits<Major_t>::max();
auto const min_minor = std::numeric_limits<Minor_t>::min();
auto const max_minor = std::numeric_limits<Minor_t>::max();

auto const min_version = Service_interface_identifier::Version{min_major, min_minor};
auto const max_version = Service_interface_identifier::Version{max_major, max_minor};

auto const default_interface = Service_interface_identifier{id0, min_version};

std::string readable_test_names(
    TestParamInfo<Interface_configuration_and_expected_connection> const& param) {
    auto const& client_conf = std::get<0>(param.param);
    auto const& server_conf = std::get<1>(param.param);
    auto const& expected_connection = std::get<2>(param.param);
    std::stringstream ss;
    ss << "id_"
       << (client_conf.get_interface().id == server_conf.get_interface().id ? "match" : "mismatch");

    if (client_conf.get_interface().version == server_conf.get_interface().version) {
        ss << "_and_version_match";
    } else {
        ss << "_and_major_version_"
           << (client_conf.get_interface().version.major ==
                       server_conf.get_interface().version.major
                   ? "match"
                   : "mismatch");
        ss << "_and_client_minor_version_is_"
           << (client_conf.get_interface().version.minor <=
                       server_conf.get_interface().version.minor
                   ? "lower_equal"
                   : "greater");
    }
    ss << "_expect_" << expected_connection;
    ss << "_" << param.index;

    return ss.str();
}

INSTANTIATE_TEST_SUITE_P(
    Identifier, InterfaceCompatibilityTest,
    Values(create_conf(default_interface, default_interface, Expectation::connect),
           create_conf(default_interface, Service_interface_identifier{id1, min_version},
                       Expectation::no_connect),
           create_conf(Service_interface_identifier{empty_id, min_version},
                       Service_interface_identifier{empty_id, min_version}, Expectation::connect),
           create_conf(Service_interface_identifier{huge_id, min_version},
                       Service_interface_identifier{huge_id, min_version}, Expectation::connect)),
    readable_test_names);

INSTANTIATE_TEST_SUITE_P(
    Version, InterfaceCompatibilityTest,
    Values(create_conf(default_interface, default_interface, Expectation::connect),
           create_conf(Service_interface_identifier{id0, {10, min_minor}}, default_interface,
                       Expectation::no_connect),
           create_conf(default_interface, Service_interface_identifier{id0, {10, min_minor}},
                       Expectation::no_connect),
           create_conf(Service_interface_identifier{id0, {min_major, 10}}, default_interface,
                       Expectation::no_connect),
           create_conf(default_interface, Service_interface_identifier{id0, {min_major, 10}},
                       Expectation::connect),
           create_conf(Service_interface_identifier{id0, {max_major, min_minor}}, default_interface,
                       Expectation::no_connect),
           create_conf(default_interface, Service_interface_identifier{id0, {max_major, min_minor}},
                       Expectation::no_connect),
           create_conf(Service_interface_identifier{id0, {min_major, max_minor}}, default_interface,
                       Expectation::no_connect),
           create_conf(default_interface, Service_interface_identifier{id0, {min_major, max_minor}},
                       Expectation::connect),
           create_conf(Service_interface_identifier{id0, max_version},
                       Service_interface_identifier{id0, max_version}, Expectation::connect)),
    readable_test_names);

}  // namespace score::socom
