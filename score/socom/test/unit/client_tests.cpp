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

#include <unistd.h>

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "score/socom/client_connector.hpp"
#include "score/socom/clients_t.hpp"
#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/server_t.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/single_connection_test_fixture.hpp"
#include "score/socom/socom_mocks.hpp"
#include "score/socom/temporary_event_subscription.hpp"
#include "score/socom/utilities.hpp"

using namespace std::literals::chrono_literals;

using score::Blank;
using ::testing::_;
using ::testing::Assign;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::StaticAssertTypeEq;
using ::testing::StrictMock;
using ::testing::TestWithParam;
using ::testing::Values;

namespace score::socom {
bool operator==(Client_connector const& /*lhs*/, Client_connector const& /*rhs*/) {
    ADD_FAILURE();
    return false;
}

Method_call_reply_data_opt get_value(std::future<Method_call_reply_data_opt> reply_future) {
    SCOPED_TRACE("get_value");
    EXPECT_TRUE(reply_future.valid());
    reply_future.wait();
    // maybe handle exceptions
    return reply_future.get();
}

#ifdef WITH_SOCOM_DEADLOCK_DETECTION

Client_connector::Uptr create_connected_client_with_event_subscription(
    Connector_factory& connector_factory, Server_data& server,
    Client_connector_callbacks_mock& cc_callbacks, Event_id const event_id) {
    std::atomic<bool> available{false};

    EXPECT_CALL(cc_callbacks, on_service_state_change(_, Service_state::available, _))
        .WillOnce(Assign(&available, true));
    auto client0 = connector_factory.create_client_connector(cc_callbacks);
    wait_for_atomics(available);

    auto const& subscribed =
        server.expect_on_event_subscription_change(event_id, Event_state::subscribed);
    EXPECT_TRUE(client0->subscribe_event(event_id, Event_mode::update));
    wait_for_atomics(subscribed);

    return client0;
}

std::function<void(Client_connector const&, Service_state, Service_interface_definition)>
delete_client_state_change(std::unique_ptr<Client_connector>& client0) {
    return [&client0](auto const& /* client_connector */, auto /* state */,
                      auto const& /* configuration */) { client0.reset(); };
}

std::function<void(Client_connector const& /*cc*/, Event_id const /*eid*/, Payload const& /*pl*/)>
delete_client(std::unique_ptr<Client_connector>& client0) {
    auto const delete_client = [&client0](Client_connector const& /*cc*/, Event_id const /*eid*/,
                                          Payload const& /*pl*/) { client0.reset(); };
    return delete_client;
}

#endif

namespace test_values {

template <typename T>
struct Test_member_pair {
    T const client_id;
    T const server_id;
};

using Test_method_id = Test_member_pair<Method_id>;
using Test_event_id = Test_member_pair<Event_id>;
using Test_method_ids = std::vector<Test_method_id>;
using Test_event_ids = std::vector<Test_event_id>;

struct Test_setup {
    explicit Test_setup(Service_interface_definition service_interface_configuration,
                        Test_method_ids test_method_ids = {}, Test_event_ids test_event_ids = {})
        : service_interface_configuration{std::move(service_interface_configuration)},
          test_method_ids{std::move(test_method_ids)},
          test_event_ids{std::move(test_event_ids)} {}

    Service_interface_definition const service_interface_configuration;
    Test_method_ids const test_method_ids;
    Test_event_ids const test_event_ids;
};

// needed to fix valgrind. When no operator<< is defined it reads the Test_setup
// byte by byte and if there is uninitialized data due to alignment valgrind reports
// an error
std::ostream& operator<<(std::ostream& out, Test_setup const& /*ignored*/) { return out; }

Payload const real_payload = make_vector_payload(make_vector_buffer(1U, 2U, 3U, 4U));

Method_result const application_return{Application_return{}};

auto const service_interface = Service_interface_identifier{
    std::string_view{"TestInterface1"}, Service_interface_identifier::Version{1U, 2U}};
auto const service_instance = Service_instance{std::string_view{"TestInstance1"}};

Server_service_interface_definition const server_configuration{
    service_interface, to_num_of_methods(2U), to_num_of_events(3U)};

Test_setup const full_client_configuration_setup{
    test_values::server_configuration, {{0U, 0U}, {1U, 1U}}, {{0U, 0U}, {1U, 1U}, {2U, 2U}}};

Service_interface_definition const no_client_configuration{service_interface};

Test_setup const no_client_configuration_setup{
    no_client_configuration, {{0U, 0U}, {1U, 1U}}, {{0U, 0U}, {1U, 1U}, {2U, 2U}}};

// method variations
Server_service_interface_definition const method_without_event_configuration{
    service_interface, to_num_of_methods(2U), to_num_of_events(0U)};

Test_setup const method_without_event_configuration_setup{
    method_without_event_configuration, {{0U, 0U}, {1U, 1U}}, {}};

Service_interface_definition const method_subset_client_configuration{
    service_interface, to_num_of_methods(1U), to_num_of_events(3U)};

Test_setup const method_subset_client_configuration_setup{
    method_subset_client_configuration, {{0U, 0U}}, {{0U, 0U}, {1U, 1U}, {2U, 2U}}};

// event variations
Server_service_interface_definition const event_without_method_configuration{
    service_interface, to_num_of_methods(0U), to_num_of_events(3U)};

Test_setup const event_without_method_configuration_setup{
    event_without_method_configuration, {}, {{0U, 0U}, {1U, 1U}, {2U, 2U}}};

Service_interface_definition const event_subset_client_configuration{
    service_interface, to_num_of_methods(2U), to_num_of_events(2U)};

Test_setup const event_subset_client_configuration_setup{
    event_subset_client_configuration, {{0U, 0U}, {1U, 1U}}, {{0U, 0U}, {1U, 1U}}};

auto const setups =
    Values(full_client_configuration_setup, no_client_configuration_setup,
           method_without_event_configuration_setup, method_subset_client_configuration_setup,
           event_without_method_configuration_setup, event_subset_client_configuration_setup);

}  // namespace test_values

class ConstructionClientConnectorTest : public ::testing::TestWithParam<test_values::Test_setup> {
   protected:
    Service_state_change_callback_mock state_change_mock;
    Event_update_callback_mock event_update_mock;
    Event_payload_allocate_callback_mock event_payload_allocate_mock;

    score::Result<Client_connector::Uptr> const callback_missing =
        score::MakeUnexpected(Construction_error::callback_missing);

    Connector_factory connector_factory =
        Connector_factory{test_values::server_configuration, test_values::service_instance};

    Client_connector_callbacks_mock callbacks;
};

TEST_P(ConstructionClientConnectorTest, ConstructCallbackMissing) {
    std::vector<Client_connector::Callbacks> input;
    input.emplace_back(Client_connector::Callbacks{
        nullptr,
        event_update_mock.as_function(),
        event_update_mock.as_function(),
        event_payload_allocate_mock.as_function(),
    });
    input.emplace_back(Client_connector::Callbacks{
        state_change_mock.as_function(),
        nullptr,
        event_update_mock.as_function(),
        event_payload_allocate_mock.as_function(),
    });
    input.emplace_back(Client_connector::Callbacks{
        state_change_mock.as_function(),
        event_update_mock.as_function(),
        nullptr,
        event_payload_allocate_mock.as_function(),
    });
    input.emplace_back(Client_connector::Callbacks{
        state_change_mock.as_function(),
        event_update_mock.as_function(),
        event_update_mock.as_function(),
        nullptr,
    });

    for (auto& callbacks : input) {
        auto cc = connector_factory.create_client_connector_with_result(
            GetParam().service_interface_configuration, test_values::service_instance,
            std::move(callbacks));

        StaticAssertTypeEq<decltype(cc), score::Result<Client_connector::Uptr>>();
        EXPECT_EQ(cc, callback_missing);
    }
}

INSTANTIATE_TEST_SUITE_P(ConstructionClientConnectorTestInstance, ConstructionClientConnectorTest,
                         test_values::setups);

class UnconnectedClientConnectorTest : public ::testing::TestWithParam<test_values::Test_setup> {
   protected:
    Connector_factory connector_factory =
        Connector_factory{test_values::server_configuration, test_values::service_instance};

    Client_connector_callbacks_mock callbacks;
    Client_connector::Uptr cc{connector_factory.create_client_connector(
        GetParam().service_interface_configuration, test_values::service_instance, callbacks)};

    score::Result<Blank> const service_not_available =
        score::MakeUnexpected(Error::runtime_error_service_not_available);
};

TEST_P(UnconnectedClientConnectorTest, CreationOfSecondClientConnectorReturnsDuplicateError) {
    // Creation fails when Client_connector already exists for the same service instance and
    // interface
    auto second_cc = connector_factory.create_client_connector_with_result(
        GetParam().service_interface_configuration, test_values::service_instance,
        create_client_callbacks(callbacks));
    EXPECT_EQ(second_cc.error(), Construction_error::duplicate_client);

    // Freeing previous Client_connector allows creation again
    cc.reset();
    second_cc = connector_factory.create_client_connector_with_result(
        GetParam().service_interface_configuration, test_values::service_instance,
        create_client_callbacks(callbacks));
    EXPECT_TRUE(second_cc);
}

TEST_P(UnconnectedClientConnectorTest, SubscribeEventReturnsServiceNotAvailable) {
    for (auto const& input : GetParam().test_event_ids) {
        EXPECT_EQ(service_not_available, cc->subscribe_event(input.client_id, Event_mode::update));
    }
}

TEST_P(UnconnectedClientConnectorTest, UnsubscribeEventReturnsServiceNotAvailable) {
    for (auto const& input : GetParam().test_event_ids) {
        EXPECT_EQ(service_not_available, cc->unsubscribe_event(input.client_id));
    }
}

TEST_P(UnconnectedClientConnectorTest,
       UnsubscribeEventIgnoreSubscribedEventReturnsServiceNotAvailable) {
    for (auto const& input : GetParam().test_event_ids) {
        EXPECT_EQ(service_not_available, cc->subscribe_event(input.client_id, Event_mode::update));

        EXPECT_EQ(service_not_available, cc->unsubscribe_event(input.client_id));
    }
}

TEST_P(UnconnectedClientConnectorTest,
       RequestEventUpdateIgnoreSubscribedEventReturnsServiceNotAvailable) {
    for (auto const& input : GetParam().test_event_ids) {
        EXPECT_EQ(service_not_available, cc->subscribe_event(input.client_id, Event_mode::update));

        EXPECT_EQ(service_not_available, cc->request_event_update(input.client_id));
    }
}

TEST_P(UnconnectedClientConnectorTest, CallMethodReturnsServiceNotAvailable) {
    Method_reply_callback_mock mrcb;
    auto const no_connection_method =
        score::MakeUnexpected(Error::runtime_error_service_not_available);

    for (auto const& input : GetParam().test_method_ids) {
        auto const result =
            cc->call_method(input.client_id, empty_payload(),
                            Method_call_reply_data{mrcb.as_function(), std::nullopt});

        EXPECT_EQ(result, no_connection_method);
    }
}

INSTANTIATE_TEST_SUITE_P(UnconnectedClientConnectorTestInstance, UnconnectedClientConnectorTest,
                         test_values::setups);

class UnavailableServerConnectorClientConnectorTest
    : public TestWithParam<test_values::Test_setup> {
   protected:
    Connector_factory connector_factory{test_values::server_configuration,
                                        test_values::service_instance};

    Server_connector_callbacks_mock server_callbacks;
    Disabled_server_connector::Uptr server{
        connector_factory.create_server_connector(server_callbacks)};

    Client_connector_callbacks_mock callbacks;
    Client_connector::Uptr cc{connector_factory.create_client_connector(
        GetParam().service_interface_configuration, test_values::service_instance, callbacks)};

    score::Result<Blank> const service_not_available =
        score::MakeUnexpected(Error::runtime_error_service_not_available);

    Event_state const subscribed{Event_state::subscribed};
};

TEST_P(UnavailableServerConnectorClientConnectorTest, SubscribeEventReturnsServiceNotAvailable) {
    for (auto const& input : GetParam().test_event_ids) {
        EXPECT_EQ(service_not_available, cc->subscribe_event(input.client_id, Event_mode::update));
    }
}

TEST_P(UnavailableServerConnectorClientConnectorTest, UnsubscribeEventReturnsServiceNotAvailable) {
    for (auto const& input : GetParam().test_event_ids) {
        EXPECT_EQ(service_not_available, cc->unsubscribe_event(input.client_id));
    }
}

TEST_P(UnavailableServerConnectorClientConnectorTest,
       UnsubscribeEventIgnoreSubscribedEventReturnsServiceNotAvailable) {
    for (auto const& input : GetParam().test_event_ids) {
        EXPECT_EQ(service_not_available, cc->subscribe_event(input.client_id, Event_mode::update));

        EXPECT_EQ(service_not_available, cc->unsubscribe_event(input.client_id));
    }
}

TEST_P(UnavailableServerConnectorClientConnectorTest,
       RequestEventUpdateIgnoreSubscribedEventReturnsServiceNotAvailable) {
    for (auto const& input : GetParam().test_event_ids) {
        EXPECT_EQ(service_not_available, cc->subscribe_event(input.client_id, Event_mode::update));

        EXPECT_EQ(service_not_available, cc->request_event_update(input.client_id));
    }
}

TEST_P(UnavailableServerConnectorClientConnectorTest, CallMethodReturnsServiceNotAvailable) {
    Method_reply_callback_mock mrcb;
    auto const no_connection_method =
        score::MakeUnexpected(Error::runtime_error_service_not_available);

    for (auto const& input : GetParam().test_method_ids) {
        auto const result =
            cc->call_method(input.client_id, empty_payload(),
                            Method_call_reply_data{mrcb.as_function(), std::nullopt});

        EXPECT_EQ(result, no_connection_method);
    }
}

INSTANTIATE_TEST_SUITE_P(UnavailableServerConnectorClientConnectorTestInstance,
                         UnavailableServerConnectorClientConnectorTest, test_values::setups);

class ConnectedClientConnectorTest : public ::testing::TestWithParam<test_values::Test_setup> {
   protected:
    Connector_factory connector_factory{test_values::server_configuration,
                                        test_values::service_instance};

    Server_data server{connector_factory};

    std::unique_ptr<Client_data> client_owner{
        std::make_unique<Client_data>(connector_factory, GetParam().service_interface_configuration,
                                      test_values::service_instance)};

    Client_data& client = *client_owner;

    Method_reply_callback_mock reply_mock{};

    Event_state const subscribed{Event_state::subscribed};

    std::unique_ptr<Temporary_event_subscription> create_event_subscription(
        test_values::Test_event_ids::const_reference input) {
        auto const& wait_subscription =
            server.expect_on_event_subscription_change(input.server_id, Event_state::subscribed);
        auto subscription = client.create_event_subscription(input.client_id);
        wait_for_atomics(wait_subscription);

        return subscription;
    }

    void unsubscribe_event_sync(std::unique_ptr<Temporary_event_subscription> subscription,
                                test_values::Test_event_ids::const_reference input) {
        auto const& wait_unsubscribed =
            server.expect_on_event_subscription_change(input.server_id, Event_state::unsubscribed);
        subscription = nullptr;
        wait_for_atomics(wait_unsubscribed);
    }
};

TEST_P(ConnectedClientConnectorTest, CallMethodWithoutReply) {
    for (auto const& input : GetParam().test_method_ids) {
        auto reply_future =
            server.expect_and_return_method_call(input.server_id, test_values::real_payload);

        client.call_method_fire_and_forget(input.client_id, test_values::real_payload);

        auto reply_callback = get_value(std::move(reply_future));
        EXPECT_EQ(reply_callback, std::nullopt);
    }
}

TEST_P(ConnectedClientConnectorTest, CallMethodWithReply) {
    for (auto const& input : GetParam().test_method_ids) {
        auto reply_future =
            server.expect_and_return_method_call(input.server_id, test_values::real_payload);

        auto const& wait_reply = client.expect_and_call_method(
            input.client_id, test_values::real_payload, test_values::application_return);

        auto reply_callback = get_value(std::move(reply_future));
        ASSERT_NE(reply_callback, std::nullopt);

        reply_callback->reply(test_values::application_return);
        wait_for_atomics(wait_reply);
    }
}

TEST_P(ConnectedClientConnectorTest, DestroyClientConnectorPendingMethod) {
    if (GetParam().test_method_ids.empty()) {
        return;
    }

    auto const& input = GetParam().test_method_ids.at(0);

    auto reply_callback_future =
        server.expect_and_return_method_call(input.server_id, empty_payload());

    client.call_method(input.client_id, empty_payload(), reply_mock.as_function());

    auto reply_callback = get_value(std::move(reply_callback_future));

    client_owner = nullptr;

    reply_callback->reply(test_values::application_return);
}

TEST_P(ConnectedClientConnectorTest, SubscribeEvent) {
    for (auto const& input : GetParam().test_event_ids) {
        auto const& wait_subscribed =
            server.expect_on_event_subscription_change(input.server_id, Event_state::subscribed);

        auto subscription = client.create_event_subscription(input.client_id);
        wait_for_atomics(wait_subscribed);

        unsubscribe_event_sync(std::move(subscription), input);
    }
}

TEST_P(ConnectedClientConnectorTest, ClientWithEventSubscriptionUnsubscribes) {
    for (auto const& input : GetParam().test_event_ids) {
        auto const& wait_subscribed =
            server.expect_on_event_subscription_change(input.server_id, Event_state::subscribed);

        {
            auto subscription = client.create_event_subscription(input.client_id);
            wait_for_atomics(wait_subscribed);
            unsubscribe_event_sync(std::move(subscription), input);
        }

        // unsubscribed client does not receive event
        server.update_event(input.server_id, test_values::real_payload);
    }
}

TEST_P(ConnectedClientConnectorTest, UnsubscribeEvent) {
    for (auto const& input : GetParam().test_event_ids) {
        auto subscription = create_event_subscription(input);

        auto const& wait_unsubscribed =
            server.expect_on_event_subscription_change(input.server_id, Event_state::unsubscribed);

        subscription = nullptr;
        wait_for_atomics(wait_unsubscribed);
    }
}

TEST_P(ConnectedClientConnectorTest, RequestEventUpdate) {
    for (auto const& input : GetParam().test_event_ids) {
        auto subscription = create_event_subscription(input);

        auto const& wait_update_request = server.expect_update_event_request(input.server_id);

        client.request_event_update(input.client_id);
        wait_for_atomics(wait_update_request);

        unsubscribe_event_sync(std::move(subscription), input);
    }
}

TEST_P(ConnectedClientConnectorTest, OnEventUpdate) {
    for (auto const& input : GetParam().test_event_ids) {
        server.update_event(input.server_id, test_values::real_payload);

        auto subscription = create_event_subscription(input);

        auto const& wait_event_update =
            client.expect_event_update(input.client_id, test_values::real_payload);

        server.update_event(input.server_id, test_values::real_payload);
        wait_for_atomics(wait_event_update);

        unsubscribe_event_sync(std::move(subscription), input);

        server.update_event(input.server_id, test_values::real_payload);
    }
}

TEST_P(ConnectedClientConnectorTest, OnRequestedEventUpdate) {
    for (auto const& input : GetParam().test_event_ids) {
        server.update_event(input.server_id, test_values::real_payload);

        auto subscription = create_event_subscription(input);

        auto const& wait_request_update = server.expect_update_event_request(input.server_id);
        client.request_event_update(input.client_id);
        wait_for_atomics(wait_request_update);

        auto const& wait_event_update =
            client.expect_requested_event_update(input.client_id, test_values::real_payload);

        server.update_requested_event(input.server_id, test_values::real_payload);
        wait_for_atomics(wait_event_update);

        unsubscribe_event_sync(std::move(subscription), input);

        server.update_event(input.server_id, test_values::real_payload);
    }
}

INSTANTIATE_TEST_SUITE_P(ConnectedClientConnectorTestInstance, ConnectedClientConnectorTest,
                         test_values::setups);

class ClientConnectorTest : public SingleConnectionTest {
   protected:
    score::Result<Blank> const not_available =
        score::MakeUnexpected(Error::runtime_error_service_not_available);
};

TEST_F(ClientConnectorTest, ConfigurationAndInstanceIsAsExpected) {
    Client_connector_callbacks_mock callbacks;
    Client_connector::Uptr cc{connector_factory.create_client_connector(callbacks)};

    auto validate_configuration = [this](Client_connector const& cc) {
        auto const& service_cfg = cc.get_configuration();
        EXPECT_EQ(service_cfg.interface, service_interface);
        EXPECT_EQ(service_cfg.num_methods, num_methods);
        EXPECT_EQ(service_cfg.num_events, num_events);
    };

    auto validate_service_instance = [this](Client_connector const& cc) {
        EXPECT_EQ(cc.get_service_instance(), service_instance);
    };

    EXPECT_CALL(callbacks, on_service_state_change(_, Service_state::available, _))
        .WillOnce([&validate_configuration, &validate_service_instance](
                      Client_connector const& cc, auto /* state */, auto const& /* cfg */) {
            validate_configuration(cc);
            validate_service_instance(cc);
        });
    EXPECT_CALL(callbacks, on_service_state_change(_, Service_state::not_available, _));
    Server_data server{connector_factory};
}

TEST_F(ClientConnectorTest, ServiceStateIsRetrievable) {
    Client_connector_callbacks_mock callbacks;
    Client_connector::Uptr cc{connector_factory.create_client_connector(callbacks)};

    EXPECT_CALL(callbacks, on_service_state_change(_, Service_state::available, _))
        .WillOnce([](Client_connector const& cc, auto /* state */, auto const& /* cfg */) {
            EXPECT_EQ(cc.is_service_available(), true);
        });
    EXPECT_CALL(callbacks, on_service_state_change(_, Service_state::not_available, _))
        .WillOnce([](Client_connector const& cc, auto /* state */, auto const& /* cfg */) {
            EXPECT_EQ(cc.is_service_available(), false);
        });
    Server_data server{connector_factory};
}

TEST_F(ClientConnectorTest, RetrieveServerConnectorConfig) {
    Client_data client0{connector_factory, Client_data::no_connect};

    auto const& available = client0.expect_service_state_change(
        1, Service_state::available, connector_factory.get_configuration());

    Server_data server{connector_factory};
    wait_for_atomics(available);

    client0.expect_service_state_change(Service_state::not_available);
}

TEST_F(ClientConnectorTest, DifferentServiceInterfaceId) {
    auto modified_interface_id =
        connector_factory.get_configuration().get_interface().id.data() + std::to_string(1);
    auto const conf = Server_service_interface_definition{
        Service_interface_identifier{std::move(modified_interface_id),
                                     connector_factory.get_configuration().get_interface().version},
        to_num_of_methods(connector_factory.get_num_methods()),
        to_num_of_events(connector_factory.get_num_events())};
    Server_data server{connector_factory, conf, connector_factory.get_instance()};

    Client_data sanity_client{connector_factory, conf, connector_factory.get_instance()};

    Client_data client{connector_factory, Client_data::no_connect};
}

TEST_F(ClientConnectorTest, DifferentServiceInstanceId) {
    auto const modified_instance_id =
        Service_instance{connector_factory.get_instance().id.data() + std::to_string(1)};
    Server_data server{connector_factory, connector_factory.get_configuration(),
                       modified_instance_id};

    Client_data sanity_client{connector_factory, connector_factory.get_configuration(),
                              modified_instance_id};

    Client_data client{connector_factory, Client_data::no_connect};
}

TEST_F(ClientConnectorTest, DifferentServiceInterfaceMajorVersion) {
    auto const modified_major_interface = Service_interface_identifier{
        connector_factory.get_configuration().get_interface().id,
        {static_cast<uint16_t>(connector_factory.get_configuration().get_interface().version.major +
                               1),
         connector_factory.get_configuration().get_interface().version.minor}};
    auto const conf = Server_service_interface_definition{
        modified_major_interface, to_num_of_methods(connector_factory.get_num_methods()),
        to_num_of_events(connector_factory.get_num_events())};
    Server_data server{connector_factory, conf, connector_factory.get_instance()};

    {
        Client_data sanity_client{connector_factory, conf, connector_factory.get_instance()};
    }
    Client_data client{connector_factory, Client_data::no_connect};
}

TEST_F(ClientConnectorTest, BiggerServiceInterfaceMinorVersion) {
    auto const modified_minor_interface = Service_interface_identifier{
        connector_factory.get_configuration().get_interface().id,
        {connector_factory.get_configuration().get_interface().version.major,
         static_cast<uint16_t>(connector_factory.get_configuration().get_interface().version.minor -
                               1)}};

    auto const conf = Server_service_interface_definition{
        modified_minor_interface, to_num_of_methods(connector_factory.get_num_methods()),
        to_num_of_events(connector_factory.get_num_events())};
    Server_data server{connector_factory, conf, connector_factory.get_instance()};

    {
        Client_data sanity_client{connector_factory, conf, connector_factory.get_instance()};
    }
    Client_data client1{connector_factory, Client_data::no_connect};
}

TEST_F(ClientConnectorTest, SmallerServiceInterfaceMinorVersion) {
    auto const modified_minor_interface = Service_interface_identifier{
        connector_factory.get_configuration().get_interface().id,
        {connector_factory.get_configuration().get_interface().version.major,
         static_cast<uint16_t>(connector_factory.get_configuration().get_interface().version.minor +
                               1)}};

    auto const conf = Server_service_interface_definition{
        modified_minor_interface, to_num_of_methods(connector_factory.get_num_methods()),
        to_num_of_events(connector_factory.get_num_events())};
    Server_data server{connector_factory, conf, connector_factory.get_instance()};

    Client_data client1{connector_factory};
}

TEST_F(ClientConnectorTest, OnStateChangeDoMethodCall) {
    std::atomic_bool available{false};
    std::atomic_bool method_call{false};

    Client_data client0{
        connector_factory, Client_data::might_connect,
        [this, &available](auto const& client_connector, auto state, auto const& configuration) {
            EXPECT_EQ(state, Service_state::available);
            EXPECT_EQ(configuration, connector_factory.get_configuration());

            auto const handle = client_connector.call_method(method_id, empty_payload(), {});
            available = true;
        }};

    Server_connector_callbacks_mock server_callbacks;

    EXPECT_CALL(server_callbacks, on_method_call(_, method_id, payload_eq(empty_payload()), _))
        .WillOnce(DoAll(Assign(&method_call, true),
                        Return(ByMove(std::make_unique<Method_invocation>()))));

    auto server_connector = connector_factory.create_and_enable(server_callbacks);
    wait_for_atomics(available, method_call);

    client0.expect_service_state_change(Service_state::not_available);
}

TEST_F(ClientConnectorTest, OnStateChangeRequestEventUpdate) {
    std::atomic<bool> available{false};
    std::atomic<bool> event_update_request{false};

    Client_data client0{
        connector_factory, Client_data::might_connect,
        [this, &available](auto const& client_connector, auto state, auto const& configuration) {
            EXPECT_EQ(state, Service_state::available);
            EXPECT_EQ(configuration, connector_factory.get_configuration());

            available = true;

            client_connector.request_event_update(event_id);
        }};

    Server_connector_callbacks_mock server_callbacks;

    EXPECT_CALL(server_callbacks, on_event_update_request(_, event_id))
        .WillOnce(Assign(&event_update_request, true));

    auto server_connector = connector_factory.create_and_enable(server_callbacks);
    wait_for_atomics(available, event_update_request);

    client0.expect_service_state_change(Service_state::not_available);
}

TEST_F(ClientConnectorTest, GetPeerCredentialsWithoutServerReturnsServiceNotAvailable) {
    Client_connector_callbacks_mock callbacks;
    Client_connector::Uptr cc{connector_factory.create_client_connector(callbacks)};

    auto check_peer_credentials = [](Client_connector const& cc, auto /* state */,
                                     auto const& /*unused*/) {
        auto credentials = cc.get_peer_credentials();
        ASSERT_TRUE(credentials);

        EXPECT_EQ(::getuid(), credentials.value().uid);
        EXPECT_EQ(::getgid(), credentials.value().gid);
    };

    auto no_peer_credentials = [](Client_connector const& cc, auto /* state */,
                                  auto const& /*unused*/) {
        auto credentials = cc.get_peer_credentials();
        ASSERT_TRUE(!credentials.has_value());

        EXPECT_EQ(credentials.error(), Error::runtime_error_service_not_available);
    };

    EXPECT_CALL(callbacks, on_service_state_change(_, Service_state::available, _))
        .WillOnce(check_peer_credentials);
    EXPECT_CALL(callbacks, on_service_state_change(_, Service_state::not_available, _))
        .WillOnce(no_peer_credentials);
    Server_data const server{connector_factory};
}

using ClientConnectorStartingConnectionDeathTest = ClientConnectorTest;

#ifdef WITH_SOCOM_DEADLOCK_DETECTION

TEST_F(ClientConnectorStartingConnectionDeathTest,
       ClientDeletionByOnStateChangeResultsInLoggingAndTermination) {
    auto el_failure = [this]() {
        Client_connector::Uptr client0;

        Client_connector_callbacks_mock cc_callbacks;
        EXPECT_CALL(cc_callbacks, on_service_state_change(_, Service_state::available, _))
            .WillOnce(delete_client_state_change(client0));

        client0 = connector_factory.create_client_connector(cc_callbacks);
        Server_data server{connector_factory};
    };
    std::string const expected_message{"SOCom error: A callback causes the Client_connector"};
    EXPECT_DEATH(el_failure(), expected_message);
}

class ClientConnectorDeathTest : public ClientConnectorTest {
   protected:
    Client_connector_callbacks_mock cc_callbacks;
    Server_data server{connector_factory};
    std::unique_ptr<Client_connector> client0 = create_connected_client_with_event_subscription(
        connector_factory, server, cc_callbacks, event_id);
    std::string const expected_message{"SOCom error: A callback causes the Client_connector"};

    ClientConnectorDeathTest() {
        server.expect_on_event_subscription_change(event_id, Event_state::unsubscribed);
    }
};

TEST_F(ClientConnectorDeathTest, ClientDeletionByOnStateChangeResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        EXPECT_CALL(cc_callbacks, on_service_state_change(_, Service_state::not_available, _))
            .WillOnce(delete_client_state_change(client0));

        auto dsc = server.disable();
    };
    EXPECT_DEATH(el_failure(), expected_message);
}

TEST_F(ClientConnectorDeathTest, ClientDeletionByOnEventUpdateResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        EXPECT_CALL(cc_callbacks, on_event_update(_, _, _)).WillOnce(delete_client(client0));

        server.update_event(event_id, empty_payload());
    };
    EXPECT_DEATH(el_failure(), expected_message);
}

TEST_F(ClientConnectorDeathTest,
       ClientDeletionByOnRequestedEventUpdateResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        auto const& update_requested = server.expect_update_event_request(event_id);
        client0->request_event_update(event_id);
        wait_for_atomics(update_requested);

        EXPECT_CALL(cc_callbacks, on_requested_event_update(_, _, _))
            .WillOnce(delete_client(client0));

        server.update_requested_event(event_id, empty_payload());
    };
    EXPECT_DEATH(el_failure(), expected_message);
}

TEST_F(ClientConnectorDeathTest, ClientDeletionByOnMethodReplyResultsInLoggingAndTermination) {
    auto const el_failure = [this]() {
        auto reply_callback = server.expect_and_return_method_call(method_id, empty_payload());
        auto const delete_client = [this](Method_result const& /*eid*/) { client0.reset(); };
        ASSERT_TRUE(client0->call_method(method_id, empty_payload(),
                                         Method_call_reply_data{delete_client, std::nullopt}));
        ASSERT_EQ(std::future_status::ready, reply_callback.wait_for(0ms));
        reply_callback.get()->reply(Method_result{Application_return{empty_payload()}});
    };
    EXPECT_DEATH(el_failure(), expected_message);
}

#endif

class ClientConnectorOutOfBoundsTest : public SingleConnectionTest {
   protected:
    score::Result<Blank> const out_of_range =
        score::MakeUnexpected(Error::logic_error_id_out_of_range);
};

TEST_F(ClientConnectorOutOfBoundsTest, SubscribeEventWithOutOfBoundsIndexReturnsOutOfRange) {
    Server_data server{connector_factory};
    Client_connector_callbacks_mock callbacks;
    auto const client0 = connector_factory.create_and_connect(callbacks);

    EXPECT_EQ(out_of_range, (client0->subscribe_event(max_event_id + 1, Event_mode::update)));
}

TEST_F(ClientConnectorOutOfBoundsTest, UnsubscribeEventWithOutOfBoundsIndexReturnsOutOfRange) {
    Server_data server{connector_factory};
    Client_connector_callbacks_mock callbacks;
    auto const client0 = connector_factory.create_and_connect(callbacks);

    EXPECT_EQ(out_of_range, (client0->unsubscribe_event(max_event_id + 1)));
}

TEST_F(ClientConnectorOutOfBoundsTest, RequestEventUpdateWithOutOfBoundsIndexReturnsOutOfRange) {
    Server_data server{connector_factory};
    Client_connector_callbacks_mock callbacks;
    auto const client0 = connector_factory.create_and_connect(callbacks);

    EXPECT_EQ(out_of_range, (client0->request_event_update(max_event_id + 1)));
}

TEST_F(ClientConnectorOutOfBoundsTest, CallMethodWithOutOfBoundsIndexReturnsOutOfRange) {
    auto const mr = Method_result{Application_return{}};

    Server_data server{connector_factory};
    Client_connector_callbacks_mock callbacks;
    Method_reply_callback_mock mrcb;
    auto const client0 = connector_factory.create_and_connect(callbacks);

    auto const out_of_range_method = score::MakeUnexpected(Error::logic_error_id_out_of_range);
    EXPECT_EQ(out_of_range_method,
              (client0->call_method(max_method_id + 1, empty_payload(),
                                    Method_call_reply_data{mrcb.as_function(), std::nullopt})));
}

TEST_F(ClientConnectorOutOfBoundsTest, ResultCompareOperator) {
    auto const result_value = score::Result<std::uint8_t>(42U);
    auto const result_error = score::MakeUnexpected(Error::logic_error_id_out_of_range);

    EXPECT_TRUE(result_value == result_value);
    EXPECT_TRUE(result_error == result_error);
    EXPECT_FALSE(result_error == result_value);
    EXPECT_FALSE(result_value == result_error);
}

TEST_F(ClientConnectorOutOfBoundsTest, ResultCompareOperatorWithVoid) {
    auto const result_error_void = score::MakeUnexpected(Error::logic_error_id_out_of_range);
    auto const result_value_void = score::Result<Blank>();

    EXPECT_TRUE(result_value_void == result_value_void);
    EXPECT_TRUE(result_error_void == result_error_void);
    EXPECT_FALSE(result_value_void == result_error_void);
    EXPECT_FALSE(result_error_void == result_value_void);
}

}  // namespace score::socom
