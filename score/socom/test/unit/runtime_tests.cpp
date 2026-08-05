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

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <string>

#include "score/socom/bridge_t.hpp"
#include "score/socom/client_connector.hpp"
#include "score/socom/clients_t.hpp"
#include "score/socom/connector_factory.hpp"
#include "score/socom/runtime.hpp"
#include "score/socom/server_t.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/single_connection_test_fixture.hpp"
#include "score/socom/socom_mocks.hpp"
#include "score/socom/utilities.hpp"

using namespace std::chrono_literals;

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::InSequence;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

namespace score::socom {

enum class Destruction_order { requests_first, bridges_first };
using Bridge_param_tuple = std::tuple<size_t, size_t, size_t, Destruction_order, bool, bool>;
using Bridges = std::vector<std::unique_ptr<Bridge_data>>;
using Atomic_getter = std::atomic<bool> const& (Bridge_data::*)() const;

struct Bridge_param {
    size_t requests_before_bridge_creation;
    size_t requests_after_bridge_creation;
    size_t num_bridges;
    Destruction_order order;
    bool delete_and_recreate_bridges;
    bool delete_and_recreate_requests;

    explicit Bridge_param(Bridge_param_tuple const& param_tuple)
        : requests_before_bridge_creation{std::get<0>(param_tuple)},
          requests_after_bridge_creation{std::get<1>(param_tuple)},
          num_bridges{std::get<2>(param_tuple)},
          order{std::get<3>(param_tuple)},
          delete_and_recreate_bridges{std::get<4>(param_tuple)},
          delete_and_recreate_requests{std::get<5>(param_tuple)} {}

    size_t get_total_requests() const {
        return requests_before_bridge_creation + requests_after_bridge_creation;
    }

    Bridge_data::Creation_sequence get_sequence() const {
        auto const sequence = 0 == requests_before_bridge_creation
                                  ? Bridge_data::bridge_then_expect
                                  : Bridge_data::expect_then_bridge;
        return sequence;
    }

    Bridge_param with_clients_already_created() const {
        return Bridge_param{std::make_tuple(get_total_requests(), 0, num_bridges, order,
                                            delete_and_recreate_bridges,
                                            delete_and_recreate_requests)};
    }

    Bridge_data::Expect expect_or_nothing(Bridge_data::Expect const& expect) const {
        return 0 != get_total_requests() ? expect : Bridge_data::nothing;
    }
};

std::string readable_test_names_bridge(TestParamInfo<Bridge_param_tuple> const& param_info) {
    auto const param = Bridge_param{param_info.param};
    std::stringstream ss;
    ss << "with_" << param.num_bridges << "_bridges";
    if (0 != param.requests_before_bridge_creation) {
        ss << "__and_" << param.requests_before_bridge_creation << "_requests_before_bridge";
    }
    if (0 != param.requests_after_bridge_creation) {
        ss << "__and_" << param.requests_after_bridge_creation << "_requests_after_bridge";
    }
    if (Destruction_order::requests_first == param.order) {
        ss << "__and_requests_and_then_bridges_destroyed";
    } else {
        ss << "__and_bridges_and_then_requests_destroyed";
    }
    if (param.delete_and_recreate_bridges) {
        ss << "__and_deleting_and_recreating_bridges";
    }
    if (param.delete_and_recreate_requests) {
        ss << "__and_deleting_and_recreating_requests";
    }

    return ss.str();
}

template <typename T>
void pop_empty(std::vector<T>& v, std::function<bool()> const& get_atom,
               Destruction_order const& order) {
    while (!v.empty()) {
        EXPECT_FALSE((Destruction_order::requests_first == order) && get_atom());
        v.erase(std::begin(v));
    }
    wait_for_atomics(get_atom());
}

Bridges create_bridges(Bridge_param const& param, Bridge_data::Expect const& expect,
                       Connector_factory& factory) {
    auto result = Bridges{param.num_bridges};
    for (auto& bridge : result) {
        bridge = std::make_unique<Bridge_data>(param.get_sequence(),
                                               param.expect_or_nothing(expect), factory);
    }
    return result;
}

bool request_service_destroyed(Bridges const& bridges) {
    auto const request_find_service_destroyed = [](Bridges::value_type const& bridge) {
        return bridge->get_request_find_service_destroyed().load();
    };

    return std::all_of(std::begin(bridges), std::end(bridges), request_find_service_destroyed);
}

std::function<bool()> create_get_destroyed(
    Bridges const& bridges, std::function<bool(Bridges const&)> const& get_destroyed_fun) {
    auto const get_destroyed = [&bridges, get_destroyed_fun]() {
        return get_destroyed_fun(bridges);
    };
    return get_destroyed;
}

void check_construction_of_callback(
    Bridge_param const& param, Bridges const& bridges, Atomic_getter const& getter,
    std::function<void(Bridge_data const&)> const& post_check_action) {
    for (auto const& bridge : bridges) {
        if (0 != param.get_total_requests()) {
            wait_for_atomics(((*bridge).*getter)());
            post_check_action(*bridge);
        } else {
            EXPECT_FALSE(((*bridge).*getter)());
        }
    }
}

void clean_bridges(Bridges& bridges, bool const clients_destroyed) {
    if (!clients_destroyed) {
        for (auto& bridge : bridges) {
            bridge->no_destroyed_check();
        }
    }
    bridges.clear();
}

void expect_callbacks(Bridges& bridges, Bridge_data::Expect const& expect,
                      Connector_factory const& connector_factory) {
    for (auto& bridge : bridges) {
        bridge->expect_callbacks(expect, connector_factory);
    }
}

template <typename T>
void clean_test(std::vector<T>& requests, Bridges& bridges, Bridge_param const& param,
                std::function<bool()> const& get_destroyed) {
    if (Destruction_order::requests_first == param.order) {
        pop_empty(requests, get_destroyed, param.order);
        clean_bridges(bridges, true);
    } else {
        clean_bridges(bridges, false);
        pop_empty(requests, get_destroyed, param.order);
    }
}

void recreate_bridges(Bridge_param const& param, Bridge_data::Expect const& expect,
                      Atomic_getter const& getter, Bridges& bridges,
                      Connector_factory& connector_factory,
                      std::function<void(Bridge_data const&)> const& post_check_action) {
    if (param.delete_and_recreate_bridges) {
        clean_bridges(bridges, false);
        append(bridges,
               create_bridges(param.with_clients_already_created(), expect, connector_factory));
        check_construction_of_callback(param, bridges, getter, post_check_action);
    }
}

template <typename REQUESTS, typename REQUESTSCREATOR>
void recreate_requests(REQUESTS& requests, Bridges& bridges,
                       Connector_factory const& connector_factory, Bridge_param const& param,
                       Bridge_data::Expect const& expect, Atomic_getter const& getter,
                       std::function<bool()> const& get_destroyed,
                       REQUESTSCREATOR const& create_requests,
                       std::function<void(Bridge_data const&)> const& post_check_action) {
    if (param.delete_and_recreate_requests) {
        pop_empty(requests, get_destroyed, Destruction_order::requests_first);
        expect_callbacks(bridges, param.expect_or_nothing(expect), connector_factory);
        append(requests, create_requests(param.get_total_requests()));
        check_construction_of_callback(param, bridges, getter, post_check_action);
    }
}

template <typename CREATEREQUESTS>
void bridge_test_template(CREATEREQUESTS const& create_requests, Bridge_param const& param,
                          Connector_factory& connector_factory, Bridge_data::Expect const& expect,
                          std::function<bool(Bridges const&)> const& get_destroyed_fun,
                          Atomic_getter const& getter,
                          std::function<void(Bridge_data const&)> const& post_check_action) {
    auto clients = create_requests(param.requests_before_bridge_creation);

    auto bridges = create_bridges(param, expect, connector_factory);
    auto const get_destroyed = create_get_destroyed(bridges, get_destroyed_fun);

    append(clients, create_requests(param.requests_after_bridge_creation));

    check_construction_of_callback(param, bridges, getter, post_check_action);

    recreate_bridges(param, expect, getter, bridges, connector_factory, post_check_action);

    recreate_requests(clients, bridges, connector_factory, param, expect, getter, get_destroyed,
                      create_requests, post_check_action);

    clean_test(clients, bridges, param, get_destroyed);
}

TEST(RuntimeFactoryTest, DefaultConstructorWorks) {
    Runtime::Uptr const rt = create_runtime();
    EXPECT_NE(nullptr, rt);
}

class RuntimeTest : public SingleConnectionTest {
   protected:
    std::vector<Service_instance> const input_find_result{
        Service_instance{"first instance", Literal_tag{}},
        Service_instance{"second instance", Literal_tag{}}, connector_factory.get_instance()};
    Service_instance const expected_find_result{connector_factory.get_instance()};

    Request_service_function_mock rsf_mock;
};

class RuntimeLegacySubscribeFindServiceTest : public RuntimeTest {};

// NOLINTNEXTLINE(readability-redundant-string-init)
MATCHER_P(Multi_set_equal, expected, "") {
    std::multiset<Service_instance> const expected_set{expected.cbegin(), expected.cend()};
    std::multiset<Service_instance> const input_set{arg.cbegin(), arg.cend()};
    return (input_set == expected_set);
}

TEST_F(RuntimeTest, RegisterServiceBridgeWithIncompleteCallbacksReturnsCallbacksMissing) {
    auto const callback_missing = score::MakeUnexpected(Construction_error::callback_missing);

    auto const registration =
        connector_factory.register_service_bridge(Bridge_identity::make(*this), nullptr);
    EXPECT_EQ(registration, callback_missing);
}

TEST_F(RuntimeTest, RegisterServiceBridgeWitCallbacksReturnsRegistration) {
    score::Result<Service_bridge_registration> const registration =
        connector_factory.register_service_bridge(Bridge_identity::make(*this),
                                                  rsf_mock.AsStdFunction());
    ASSERT_TRUE(registration);
    EXPECT_TRUE(registration.value());
}

TEST_F(RuntimeTest, BridgeReceivesRequestServiceFunctionCallForUnknownService) {
    Bridge_data bridge{Bridge_data::bridge_then_expect, Bridge_data::request_service_function,
                       connector_factory};

    Client_data const client{connector_factory, Client_data::no_connect};
    EXPECT_TRUE(bridge.get_request_find_service_created());
}

TEST_F(RuntimeTest, BridgeDoesNotReceiveRequestServiceFunctionCallForKnownService) {
    Bridge_data bridge{Bridge_data::bridge_then_expect, Bridge_data::nothing, connector_factory};

    Server_data const server{connector_factory};
    Client_data const client{connector_factory};
    EXPECT_FALSE(bridge.get_request_find_service_created());
}

TEST_F(RuntimeTest,
       BridgeCreatesServerConnectorInRequestServiceFunctionCallbackAtClientConnectorCreation) {
    std::optional<Server_data> server;
    auto const create_server = [this, &server](
                                   Service_interface_definition const& /*configuration*/,
                                   Service_instance const& /*instance*/) {
        server.emplace(connector_factory);
    };

    Bridge_data bridge{Bridge_data::bridge_then_expect, Bridge_data::nothing, connector_factory};
    bridge.expect_request_find_service(connector_factory.get_configuration(),
                                       connector_factory.get_instance(), create_server);

    Service_state_change_callback_mock state_change_callback;
    {
        InSequence const is;
        EXPECT_CALL(state_change_callback,
                    Call(_, Service_state::available, connector_factory.get_configuration()));
    }
    Client_data const client{connector_factory, Client_data::might_connect,
                             state_change_callback.as_function()};
}

TEST_F(RuntimeTest, ConstructDuplicateReturnsDuplicateServiceError) {
    Server_connector_callbacks_mock callbacks;
    auto const scd = connector_factory.create_server_connector_with_result(callbacks);
    ASSERT_TRUE(scd.has_value());

    Server_connector_callbacks_mock callbacks_2;
    auto const scd_2 = connector_factory.create_server_connector_with_result(callbacks_2);
    score::Result<Disabled_server_connector::Uptr> const duplicate_service =
        score::MakeUnexpected(Construction_error::duplicate_service);
    EXPECT_EQ(duplicate_service, scd_2);
}

TEST_F(RuntimeTest, ConstructDuplicateMultipleTimesReturnsDuplicateServiceError) {
    Server_connector_callbacks_mock callbacks;
    auto const scd = connector_factory.create_server_connector_with_result(callbacks);
    ASSERT_TRUE(scd.has_value());

    Server_connector_callbacks_mock callbacks_2;
    auto const scd_2 = connector_factory.create_server_connector_with_result(callbacks_2);

    score::Result<Disabled_server_connector::Uptr> const duplicate_service =
        score::MakeUnexpected(Construction_error::duplicate_service);
    ASSERT_EQ(duplicate_service, scd_2);

    Server_connector_callbacks_mock callbacks_3;
    auto const scd_3 = connector_factory.create_server_connector_with_result(callbacks_3);

    EXPECT_EQ(duplicate_service, scd_3);
}

TEST_F(RuntimeTest, CreatingServerConnectorDeletingItAndRecreatingReturnsValidServerConnector) {
    Server_connector_callbacks_mock callbacks;
    {
        auto const scd = connector_factory.create_server_connector_with_result(callbacks);
        ASSERT_TRUE(scd.has_value());
        Server_connector_callbacks_mock callbacks_2;
        score::Result<Disabled_server_connector::Uptr> const duplicate_service =
            score::MakeUnexpected(Construction_error::duplicate_service);
        EXPECT_EQ(duplicate_service,
                  connector_factory.create_server_connector_with_result(callbacks_2));
    }

    {
        auto const scd = connector_factory.create_server_connector_with_result(callbacks);
        ASSERT_TRUE(scd.has_value());
    }
}

TEST_F(RuntimeTest, ConnectorDestroyedAfterRuntimeDoesNotCrash) {
    Disabled_server_connector::Uptr connector{nullptr};
    {
        Connector_factory factory{connector_factory};
        Server_connector_callbacks_mock callbacks;
        connector = factory.create_server_connector(callbacks);
    }
}

class RuntimeBridgeTest : public RuntimeTest, public WithParamInterface<Bridge_param_tuple> {
   protected:
    Bridge_param m_param = Bridge_param{GetParam()};
};

TEST_P(RuntimeBridgeTest, CreationOfClientsWillCallRequestServiceFunction) {
    auto const create_requests = [this](size_t num_requests) {
        return Client_data::create_clients(connector_factory, num_requests,
                                           Client_data::no_connect);
    };

    auto const post_check = [](Bridge_data const& /*bridge*/) {};

    bridge_test_template(create_requests, m_param, connector_factory,
                         Bridge_data::request_service_function, request_service_destroyed,
                         &Bridge_data::get_request_find_service_created, post_check);
}

INSTANTIATE_TEST_SUITE_P(RequestsAfterBridgeCreation, RuntimeBridgeTest,
                         Combine(Values(0), Values(0, 1), Values(1, 10),
                                 Values(Destruction_order::requests_first,
                                        Destruction_order::bridges_first),
                                 Bool(), Bool()),
                         readable_test_names_bridge);

}  // namespace score::socom
