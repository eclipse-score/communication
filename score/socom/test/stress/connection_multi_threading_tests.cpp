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
#include <cstddef>
#include <utility>

#include "gtest/gtest.h"
#include "score/socom/client_connector.hpp"
#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/multi_threaded_test_template.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/single_connection_test_fixture.hpp"

namespace score::socom
{

class Event_method_counter
{
    const std::size_t m_min_events_received = 1000;
    const std::size_t m_min_method_call_responses_received = 1000;
    std::atomic<std::size_t> m_events_received{0};
    std::atomic<std::size_t> m_method_call_responses_received{0};

  public:
    void event_received()
    {
        m_events_received++;
    }
    std::size_t num_events_received() const
    {
        return m_events_received;
    }

    void method_response_received()
    {
        m_method_call_responses_received++;
    }
    std::size_t num_method_responses_received() const
    {
        return m_method_call_responses_received;
    }

    Stop_condition create_stop_condition() const
    {
        return [this]() {
            return num_events_received() >= m_min_events_received &&
                   num_method_responses_received() >= m_min_method_call_responses_received;
        };
    }
};

void subscribe_events(const Client_connector& cc, const std::size_t num_events)
{
    for (std::size_t i{0}; i < num_events; i++)
    {
        const auto event_mode = 0 == i % 2 ? Event_mode::update : Event_mode::update_and_initial_value;
        cc.subscribe_event(i, event_mode);
    }
}

void call_methods(const Client_connector& cc,
                  const std::size_t num_methods,
                  const Payload& payload,
                  Method_reply_callback on_method_reply)
{
    for (std::size_t i{0}; i < num_methods; i++)
    {
        auto reply_cb = 0 == i % 2 ? Method_call_reply_data_opt{std::in_place, std::move(on_method_reply), std::nullopt}
                                   : Method_call_reply_data_opt{std::nullopt};
        (void)cc.call_method(i, clone_payload(payload), std::move(reply_cb));
    }
}

class ConnectionMultiThreadingTest : public SingleConnectionTest
{
  protected:
    Event_method_counter counter;

    Method_reply_callback create_method_reply_callback()
    {
        return [this](const Method_result& /*result*/) {
            counter.method_response_received();
        };
    }

    Disabled_server_connector::Callbacks create_server_callbacks()
    {
        return {[](Enabled_server_connector& /*esc*/,
                   Method_id /*mid*/,
                   Payload /*pl*/,
                   const Method_call_reply_data_opt& reply,
                   const auto& /*cred*/) {
                    if (reply)
                    {
                        reply->reply(Method_result{Application_return{}});
                    }
                    return Method_invocation::Uptr{};
                },
                [this](Enabled_server_connector& esc, const Event_id& eid, const Event_state& /*state*/) {
                    esc.update_event(eid, clone_payload(real_payload));
                },
                [this](Enabled_server_connector& esc, const Event_id& eid) {
                    esc.update_event(eid, clone_payload(real_payload));
                },
                [](Enabled_server_connector& /*esc*/, const Method_id& /*mid*/) -> score::Result<Writable_payload> {
                    ADD_FAILURE() << "Unexpected call to on_method_call_payload_allocate for method_id " << event_id;
                    return score::MakeUnexpected(Server_connector_error::logic_error_id_out_of_range);
                }};
    }  // namespace score::socom

    const std::function<void()> server_thread = [this]() {
        auto esc = Disabled_server_connector::enable(
            this->connector_factory.create_server_connector(create_server_callbacks()));

        for (std::size_t i{0}; i < connector_factory.get_num_events(); i++)
        {
            esc->update_event(i, clone_payload(real_payload));
        }
    };

    Client_connector::Callbacks create_client_callbacks(socom::Service_state_change_callback on_state_change)
    {
        return {std::move(on_state_change),
                [this](const Client_connector& /*cc*/, const Event_id& /*eid*/, Payload /*pl*/) {
                    counter.event_received();
                },
                [this](const Client_connector& /*cc*/, const Event_id& /*eid*/, Payload /*pl*/) {
                    counter.event_received();
                },
                [](const Client_connector& /*cc*/, const Event_id& event_id) {
                    ADD_FAILURE() << "Unexpected call to on_event_payload_allocate for event_id " << event_id;
                    return score::MakeUnexpected(Server_connector_error::runtime_error_no_client_subscribed_for_event);
                }};
    }
};

TEST_F(ConnectionMultiThreadingTest, ClientSubscribesEventsAndCallsMethodsInStateChangeCallbackWithoutRace)
{
    auto on_state_change =
        [this](const Client_connector& cc, const Service_state& state, const Service_interface_definition& /*conf*/) {
            if (Service_state::available == state)
            {
                subscribe_events(cc, connector_factory.get_num_events());
                call_methods(cc, connector_factory.get_num_methods(), real_payload, create_method_reply_callback());
            }
        };

    const auto client_thread = [this, on_state_change]() {
        const auto cc = this->connector_factory.create_client_connector(create_client_callbacks(on_state_change));
        (void)cc;
    };

    multi_threaded_test_template({client_thread, server_thread}, counter.create_stop_condition());
}

TEST_F(ConnectionMultiThreadingTest, ClientSubscribesEventsServerThreadAndCallsMethodsInOwnThread)
{
    auto on_state_change =
        [this](const Client_connector& cc, const Service_state& state, const Service_interface_definition& /*conf*/) {
            if (Service_state::available == state)
            {
                subscribe_events(cc, connector_factory.get_num_events());
            }
        };

    const auto client_thread = [this, on_state_change]() {
        const auto cc = this->connector_factory.create_client_connector(create_client_callbacks(on_state_change));
        call_methods(*cc, connector_factory.get_num_methods(), real_payload, create_method_reply_callback());
    };

    multi_threaded_test_template({client_thread, server_thread}, counter.create_stop_condition());
}

TEST_F(ConnectionMultiThreadingTest, ClientCallsMethodsInServerThreadAndSubscribesEventsInOwnThreadWithoutRace)
{
    auto on_state_change =
        [this](const Client_connector& cc, const Service_state& state, const Service_interface_definition& /*conf*/) {
            if (Service_state::available == state)
            {
                call_methods(cc, connector_factory.get_num_methods(), real_payload, create_method_reply_callback());
            }
        };

    const auto client_thread = [this, on_state_change]() {
        const auto cc = this->connector_factory.create_client_connector(create_client_callbacks(on_state_change));
        subscribe_events(*cc, connector_factory.get_num_events());
    };

    multi_threaded_test_template({client_thread, server_thread}, counter.create_stop_condition());
}

TEST_F(ConnectionMultiThreadingTest, ClientCallsMethodsAndSubscribesEventsInOwnThreadWithoutRace)
{
    auto on_state_change = [](const Client_connector& /* cc */,
                              const Service_state& /* state */,
                              const Service_interface_definition& /*conf*/) {};

    const auto client_thread = [this, on_state_change]() {
        const auto cc = this->connector_factory.create_client_connector(create_client_callbacks(on_state_change));
        subscribe_events(*cc, connector_factory.get_num_events());
        call_methods(*cc, connector_factory.get_num_methods(), real_payload, create_method_reply_callback());
    };

    multi_threaded_test_template({client_thread, server_thread}, counter.create_stop_condition());
}

TEST_F(ConnectionMultiThreadingTest, ServerConnectsToClientWhileClientDtorIsRunningWithoutDeadlock)
{
    std::mutex mtx_client_destroyed;
    std::condition_variable cv_client_destroyed;
    std::atomic<bool> client_destroyed{true};
    const std::size_t num_connection_limit{5000};

    struct State
    {
        std::mutex mtx_connected;
        std::condition_variable cv_connected;
        bool connected = false;
        std::atomic<std::size_t> num_connected{0};
    } on_state_change_vars;

    const auto on_state_change = [&on_state_change_vars](const Client_connector& /* cc */,
                                                         const Service_state& state,
                                                         const Service_interface_definition& /*conf*/) {
        on_state_change_vars.num_connected += static_cast<std::size_t>(Service_state::available == state);
        if (Service_state::available == state)
        {
            std::unique_lock<std::mutex> lock_connected(on_state_change_vars.mtx_connected);
            on_state_change_vars.connected = true;
            on_state_change_vars.cv_connected.notify_one();  // Notify client_thread that connection is established
        }
    };

    const auto client_thread = [this,
                                on_state_change,
                                &client_destroyed,
                                &on_state_change_vars,
                                &cv_client_destroyed,
                                &mtx_client_destroyed]() {
        {
            client_destroyed = false;
            const auto cc = this->connector_factory.create_client_connector(create_client_callbacks(on_state_change));

            // Wait for server to connect
            std::unique_lock<std::mutex> lock_connected(on_state_change_vars.mtx_connected);
            if (!on_state_change_vars.connected)
            {
                on_state_change_vars.cv_connected.wait(lock_connected);
            }
        }

        std::unique_lock<std::mutex> lock_destroyed(mtx_client_destroyed);
        client_destroyed = true;
        cv_client_destroyed.notify_one();  // Notify server_thread that client is destroyed
    };

    const auto server_thread = [this, &client_destroyed, &cv_client_destroyed, &mtx_client_destroyed]() {
        // Exit while client is marked as destroyed, Multi_threaded_test_template will run us again.
        // We need to do it like this in order to handle cases where server_thread already
        // starts, but the stop condition prevents creation of a new client_thread.
        if (client_destroyed)
        {
            return;
        }

        const auto esc =
            Disabled_server_connector::enable(connector_factory.create_server_connector(create_server_callbacks()));

        std::unique_lock<std::mutex> lock_destroyed(mtx_client_destroyed);
        // Wait for client to be destroyed
        if (!client_destroyed)
        {
            cv_client_destroyed.wait(lock_destroyed);
        }
    };

    multi_threaded_test_template({client_thread, server_thread}, [&on_state_change_vars]() {
        return on_state_change_vars.num_connected > num_connection_limit;
    });
}

}  // namespace score::socom
