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

#include <atomic>
#include <mutex>

#include "score/socom/clients_t.hpp"
#include "score/socom/multi_threaded_test_template.hpp"
#include "score/socom/single_connection_test_fixture.hpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Assign;

namespace score::socom {

struct State_data {
    // State_data can be updated concurrently by Server_connector and Client_connector
    std::mutex m_mutex;
    std::optional<Service_state> m_old_state;
    bool m_not_available{false};
    bool m_available{false};

    void check_state_machine(Service_state const& state) const {
        // check state machine transitions
        // First state is always not_available
        if (!m_old_state) {
            return;
        }

        // not_available -> available|not_available
        EXPECT_TRUE((Service_state::not_available != *m_old_state) ||
                    (Service_state::available == state) || (Service_state::not_available == state))
            << "old_state == " << *m_old_state << ", state == " << state;
        // available -> not_available
        EXPECT_TRUE((Service_state::available != *m_old_state) ||
                    (Service_state::not_available == state))
            << "old_state == " << *m_old_state << ", state == " << state;
    }

    bool update_reached_states(Service_state const& state) {
        m_old_state = state;
        m_not_available = m_not_available || Service_state::not_available == state;
        m_available = m_available || Service_state::available == state;
        return m_not_available && m_available;
    }
};

class Client_to_connect {
    std::atomic<bool> m_client_connected{false};
    Service_state_change_callback_mock m_state_change_mock;

   public:
    using Callback_creator_t = std::function<Service_state_change_callback()>;

    Client_to_connect() {
        EXPECT_CALL(m_state_change_mock, Call(_, _, _))
            .Times(AnyNumber())
            .WillRepeatedly(Assign(&m_client_connected, true));
    }

    void reset() { m_client_connected = false; }

    bool is_satisfied() { return m_client_connected; }

    Loop_function_t create_thread_function(Connector_factory& factory) {
        return create_thread_function(factory, factory.get_configuration(), factory.get_instance());
    }

    Loop_function_t create_thread_function(Connector_factory& factory, Callback_creator_t cbcrtr) {
        reset();
        return [&factory, cbcrtr = std::move(cbcrtr)]() {
            Client_data client{factory, Client_data::might_connect, cbcrtr()};
        };
    }

    Loop_function_t create_thread_function(Connector_factory& factory,
                                           Server_service_interface_definition const& configuration,
                                           Service_instance const& instance) {
        reset();
        return [this, &factory, &configuration, &instance]() {
            Client_data client{factory, Client_data::might_connect, configuration, instance,
                               m_state_change_mock.as_function()};
        };
    }

    Service_state_change_callback create_callback() {
        return [this, data = std::make_shared<State_data>()](auto const& /*cc*/, auto state,
                                                             auto const& /*conf*/) {
            std::unique_lock<std::mutex> const lock{data->m_mutex};
            data->check_state_machine(state);
            m_client_connected = data->update_reached_states(state);
        };
    }

    Callback_creator_t callback_creator() {
        return [this]() { return create_callback(); };
    }
};

class RuntimeMultiThreadingTest : public SingleConnectionTest {
   protected:
    std::vector<Service_instance> const input_find_result{
        Service_instance{"first instance", Literal_tag{}},
        Service_instance{"second instance", Literal_tag{}}, connector_factory.get_instance()};

    Request_service_function_mock rsf_mock;

    Loop_function_t create_servers_thread_main() {
        return [this]() { Server_data server{connector_factory}; };
    }

    Loop_function_t create_servers_thread_main(
        Server_service_interface_definition const& configuration,
        Service_instance const& instance) {
        return [this, &configuration, &instance]() {
            Server_data server{connector_factory, configuration, instance};
        };
    }

    Loop_function_t create_bridges_thread_main() {
        auto const register_bridges = [this, rsf_mock_function = rsf_mock.AsStdFunction()]() {
            auto const registration = connector_factory.register_service_bridge(
                Bridge_identity::make(*this), rsf_mock_function);
            ASSERT_TRUE(registration);
        };
        return register_bridges;
    }
};

TEST_F(RuntimeMultiThreadingTest, CreationOfServerAndClientConnectorsHasNoRaceCondition) {
    Client_to_connect client;

    auto const start_clients = client.create_thread_function(connector_factory);

    multi_threaded_test_template({create_servers_thread_main(), start_clients},
                                 [&client]() { return static_cast<bool>(client.is_satisfied()); });
}

TEST_F(RuntimeMultiThreadingTest, CreationOfServerAndClientConnectorsCallsCallbacksInCorrectOrder) {
    Client_to_connect client;

    auto const start_clients =
        client.create_thread_function(connector_factory, client.callback_creator());

    multi_threaded_test_template({create_servers_thread_main(), start_clients},
                                 [&client]() { return static_cast<bool>(client.is_satisfied()); });
}

TEST_F(RuntimeMultiThreadingTest,
       BridgesWhichCreateServerConnectorsAndClientConnectorsHaveNoRaceCondition) {
    EXPECT_CALL(rsf_mock, Call(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto const& configuration, auto const& /*unused*/) {
            static std::atomic_int instance_number{0};
            auto instance = Service_instance{std::to_string(instance_number++)};
            Server_data server{
                connector_factory,
                Server_service_interface_definition{configuration.interface,
                                                    to_num_of_methods(configuration.num_methods),
                                                    to_num_of_events(configuration.num_events)},
                instance};
            return nullptr;
        });

    std::atomic<bool> client_connected{false};
    Service_state_change_callback_mock state_change_mock;
    EXPECT_CALL(state_change_mock, Call(_, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(Assign(&client_connected, true));
    auto const start_clients = [this, &state_change_mock]() {
        Client_data client{connector_factory, Client_data::might_connect,
                           state_change_mock.as_function()};
    };

    multi_threaded_test_template(
        {create_bridges_thread_main(), start_clients},
        [&client_connected]() { return static_cast<bool>(client_connected); });
}

TEST_F(RuntimeMultiThreadingTest, BridgesAndClientConnectorsHaveNoRaceCondition) {
    EXPECT_CALL(rsf_mock, Call(_, _)).Times(AnyNumber());

    auto const start_clients = [this]() {
        Client_data client{connector_factory, Client_data::no_connect};
    };

    multi_threaded_test_template({create_bridges_thread_main(), start_clients},
                                 []() { return true; });
}

}  // namespace score::socom
