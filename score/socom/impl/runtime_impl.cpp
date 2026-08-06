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

#include "runtime_impl.hpp"

#include <score/assert.hpp>
#include <unistd.h>
#include <map>
#include <mutex>
#include <optional>
#include <set>

#include <algorithm>
#include <iterator>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "client_connector_impl.hpp"
#include "score/mw/log/logging.h"
#include "score/result/result.h"
#include "score/socom/client_connector.hpp"
#include "score/socom/error.hpp"
#include "score/socom/final_action.hpp"
#include "score/socom/impl/runtime_registration.hpp"
#include "score/socom/impl/service_identifier.hpp"
#include "score/socom/posix_credentials.hpp"
#include "score/socom/runtime.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/service_interface_identifier.hpp"
#include "server_connector_impl.hpp"

namespace score::socom
{

namespace
{

bool is_matching_instance(const std::optional<Service_instance>& filter, const Service_instance& instance)
{
    return !filter || *filter == instance;
}

template <typename Map>
std::vector<typename Map::key_type> get_keys(const Map& map)
{
    std::vector<typename Map::key_type> keys;
    keys.reserve(map.size());
    for (const auto& pair : map)
    {
        keys.emplace_back(pair.first);
    }
    return keys;
}

/// \brief Removes key from map if value is empty
///
/// \param map Map to clean up.
/// \param key Key to remove from map if value is empty.
/// \param value Value associated with the key.
template <template <typename, typename> class Map, typename Key, typename Value>
void cleanup(Map<Key, Value>& map, const Key& key, const Value& value) noexcept
{
    if (value.empty())
    {
        map.erase(key);
    }
}

/// \brief Removes key from inner maps of map
///
/// \param map Map to clean up.
/// \param key Key to remove from inner maps.
template <typename Key0, typename Key1, typename Value>
void cleanup(
    std::map<Key0, std::tuple<std::weak_ptr<std::map<Key1, Value>>, std::vector<std::optional<Bridge_identity>>>>& map,
    const Key1& key) noexcept
{
    for (const auto& values : map)
    {
        const auto value_locked = std::get<0>(values.second).lock();

        if (nullptr != value_locked)
        {
            value_locked->erase(key);
        }
        // Cannot be covered, there is no reliable way to delete values.second other than using
        // stop_registration which calls this cleanup-function.
        // Sporadically covered by
        // TEST_F(RuntimeMultiThreadingTest,
        //     BridgesAndSubscribeFindServiceHaveNoRaceConditions)
        else
        {
        }
    }
}

/// \brief Removes key from map if its pointed to value is empty
///
/// \param map Map to clean up.
/// \param key Key to remove from map if its pointed to value is empty.
template <typename Key, typename Value>
void cleanup(std::map<Key, std::tuple<std::weak_ptr<Value>, std::vector<std::optional<Bridge_identity>>>>& map,
             const Key& key) noexcept
{
    const auto request = map.find(key);
    // Cannot be covered, there is no reliable way to delete key from map other than using
    // stop_subscription which calls this cleanup-function.
    // Sporadically covered by
    // TEST_F(RuntimeMultiThreadingTest,
    // CreationOfMultipleServerAndClientConnectorsHasNoRaceCondition)

    if (std::end(map) == request)
    {
        return;
    }

    const auto request_locked = std::get<0>(request->second).lock();
    if (nullptr == request_locked)
    {
        map.erase(request);
    }
}

class Final_action_registration final : public IRegistration
{
    // will be executed as a final action on destruction.
    Final_action m_final_action;

  public:
    explicit Final_action_registration(Final_action final_action) : m_final_action{std::move(final_action)} {}
};

class Registration_collection final : public IRegistration
{
  public:
    Registration_collection(Registration r1, Registration r2)
        : m_registration0{std::move(r1)}, m_registration1{std::move(r2)}
    {
    }

  private:
    Registration m_registration0;
    Registration m_registration1;
};

bool is_minor_version_compatible(const Service_interface_identifier& server, const Service_interface_identifier& client)
{
    return client.version.minor <= server.version.minor;
}

bool is_interface_compatible(const Service_interface_identifier& server, const Service_interface_identifier& client)
{
    // Defensive programming. This function is called in the two register_connector functions.
    // First, a record is loaded depending on service interface and instance. The record ensures
    // that the right server/client is loaded, therefore this function will always return true.

    return (server.id == client.id) && (server.version.major == client.version.major) &&
           is_minor_version_compatible(server, client);
}

bool is_valid(const Client_connector::Callbacks& callbacks)
{
    return !callbacks.on_service_state_change.empty() && !callbacks.on_event_update.empty() &&
           !callbacks.on_event_requested_update.empty() && !callbacks.on_event_payload_allocate.empty();
}

bool is_valid(const Disabled_server_connector::Callbacks& callbacks)
{
    return !callbacks.on_method_call.empty() && !callbacks.on_event_update_request.empty() &&
           !callbacks.on_event_subscription_change.empty() && !callbacks.on_method_call_payload_allocate.empty();
}

// actually understandable and easily reviewed, a code change is not justified.
template <typename Instance, typename Callback, typename CreateValue>
void register_bridge(const Bridge_registration_id& bridge_id,
                     std::unique_lock<std::mutex>& bridge_lock,
                     const Active_bridge_requests<Instance, Callback>& bridge_requests,
                     const CreateValue& create_value)
{
    using Abr = Active_bridge_requests<Instance, Callback>;
    using Key_t = typename Abr::key_type;
    using Value_t = typename std::tuple_element<0, typename Abr::mapped_type>::type::element_type::mapped_type;
    SCORE_LANGUAGE_FUTURECPP_ASSERT(bridge_lock.owns_lock());

    /// THE algorithm:
    // copy bridge_requests
    std::set<Key_t> requests_done;
    auto requests_to_do = get_keys(bridge_requests);
    bool first_run = true;
    // would have been do {} while(); but static code analysis forbids it
    while (!requests_to_do.empty() || first_run)
    {
        first_run = false;
        // unlock
        bridge_lock.unlock();

        // call callbacks and store result in copy
        std::map<Key_t, Value_t> request_with_callback_result;
        for (const auto& request : requests_to_do)
        {
            request_with_callback_result[request] = create_value(request);
            requests_done.insert(request);
        }
        requests_to_do.clear();

        // lock
        bridge_lock.lock();

        // merge and compute difference
        for (const auto& request : bridge_requests)
        {
            const auto cb_result_iter = request_with_callback_result.find(request.first);
            if (std::end(request_with_callback_result) == cb_result_iter)
            {
                // ensure each request is only processed once
                if (std::end(requests_done) == requests_done.find(request.first))
                {
                    // new request at Runtime-API was done. Need to add it to new bridge as well
                    requests_to_do.emplace_back(request.first);
                }
            }
            else
            {
                const auto bridge_to_request = std::get<0>(request.second).lock();
                // if destruction of Client_connector is concurrently happening there might be a
                // nullptr in the map until the map is cleaned. Thus because the map is going to be
                // cleaned do not bother to call the callback of the bridge

                if (nullptr != bridge_to_request)
                {
                    bridge_to_request->emplace(bridge_id, std::move(cb_result_iter->second));
                }
                // Cannot be covered, there is no reliable way to delete request.second during the
                // unlocked create_value-loop.
                // Sporadically covered by
                // TEST_F(RuntimeMultiThreadingTest,
                //     BridgesAndSubscribeFindServiceHaveNoRaceConditions)
                else
                {
                }
            }
        }
        // repeat with new requests
    }

    // leave function locked
    SCORE_LANGUAGE_FUTURECPP_ASSERT(bridge_lock.owns_lock());
}

bool is_forward_subscription(const std::optional<Bridge_identity>& identity,
                             const Bridge_identity& bridge_callback_identity,
                             const std::vector<std::optional<Bridge_identity>>& subscriber_identity_record)
{
    const bool is_first_subscription = subscriber_identity_record.size() == 1U;
    const bool is_second_subscription = subscriber_identity_record.size() == 2U;
    const bool is_current_subscriber = identity && (*identity == bridge_callback_identity);

    bool forward_subscription = false;
    if (is_first_subscription)
    {
        forward_subscription = !is_current_subscriber;
    }
    else if (is_second_subscription)
    {
        const bool is_already_subscribed = (std::find(subscriber_identity_record.begin(),
                                                      subscriber_identity_record.end(),
                                                      bridge_callback_identity) != subscriber_identity_record.end());

        forward_subscription = is_already_subscribed && !is_current_subscriber;
    }
    else
    {
        // Nothing to do: satisfies AutosarC++18_10-M6.4.2
    }
    return forward_subscription;
}

template <typename ReturnValue, typename Instance, typename Instance1, typename Handle, typename CreateValue>
std::shared_ptr<ReturnValue> get_bridge_requests(
    const Service_interface_definition& configuration,
    const Instance& instance,
    std::optional<Bridge_identity> identity,
    std::unique_lock<std::mutex>& bridge_lock,
    Active_bridge_requests<Instance1, Handle>& active_requests,
    const Runtime_impl::Bridge_registration_to_callbacks& bridge_to_callback,
    const CreateValue& create_value)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT(bridge_lock.owns_lock());
    const auto key = std::make_tuple(configuration, instance);
    const auto find_services = active_requests.find(key);
    std::shared_ptr<ReturnValue> result =
        (std::end(active_requests) == find_services) ? nullptr : std::get<0>(find_services->second).lock();

    if (nullptr == result)
    {
        result = std::make_shared<ReturnValue>();
        active_requests[key] = std::make_tuple(result, std::vector<std::optional<Bridge_identity>>{});
    }

    auto& subscriber_identity_record = std::get<1>(active_requests[key]);

    // Copy is intentional: callbacks are invoked without holding bridge_lock and the
    // original container may change concurrently.
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    const auto bridge_to_callback_copy = bridge_to_callback;
    ReturnValue tmp_result;

    subscriber_identity_record.emplace_back(identity);

    for (const auto& bridge : bridge_to_callback_copy)
    {
        const bool forward_subscription =
            is_forward_subscription(identity, bridge.first->get_identity(), subscriber_identity_record);

        if (forward_subscription)
        {
            bridge_lock.unlock();
            tmp_result.emplace(bridge.first, create_value(bridge, configuration, instance));
            bridge_lock.lock();
        }
    }

    for (auto& bridge_id_callback_result : tmp_result)
    {
        // The false condition case might occur between bridge_lock.unlock() and bridge_lock.lock()
        // when a subscription is beeing forwarded. It could be observed that the false case occurs
        // sporadically with the following test:
        // TEST_F(RuntimeMultiThreadingTest, BridgesAndSubscribeFindServiceHaveNoRaceConditions)
        //
        // This leads to the assumption that we are dealing with a race condition here which does
        // not lead to any problems/errors.
        // TODO: This case should be further discussed.

        if (std::end(*result) == result->find(bridge_id_callback_result.first))
        {
            result->emplace(bridge_id_callback_result.first, std::move(bridge_id_callback_result.second));
        }
    }
    return result;
}

}  // namespace

Service_database::Service_database(std::mutex& runtime_mutex) : m_runtime_mutex{runtime_mutex} {}

Service_record& Service_database::get_record(const Service_interface_identifier& interface,
                                             const Service_instance& instance)
{
    auto it_interface = m_service_records.find(interface);
    if (it_interface == std::end(m_service_records))
    {
        it_interface = m_service_records.emplace(interface, Service_instances{}).first;
    }

    auto& record = it_interface->second.emplace(instance, Service_record{m_runtime_mutex}).first->second;

    return record;
}

Interfaces_instances Service_database::get_instances(const Service_interface_identifier& interface,
                                                     const std::optional<Service_instance>& filter) const
{
    const auto it_interface = m_service_records.find(interface);
    if (std::end(m_service_records) == it_interface)
    {
        return {};
    }
    const auto& instances = it_interface->second;
    const auto matches_filter = [&filter](const Service_instances::value_type& instance) {
        return (instance.second.is_available() && is_matching_instance(filter, instance.first));
    };

    Service_instances filtered_instances;
    (void)std::copy_if(std::begin(instances),
                       std::end(instances),
                       std::inserter(filtered_instances, std::end(filtered_instances)),
                       matches_filter);
    return {{interface, get_keys(filtered_instances)}};
}

Interfaces_instances Service_database::get_instances(const std::optional<Service_interface_identifier>& interface,
                                                     const std::optional<Service_instance>& filter) const
{
    if (interface)
    {
        return get_instances(*interface, filter);
    }

    Interfaces_instances result{};
    for (const auto& interface_with_instances : m_service_records)
    {
        auto& instances = result[interface_with_instances.first];
        for (const auto& instance : interface_with_instances.second)
        {
            instances.emplace_back(instance.first);
        }
    }
    return result;
}

Stop_subscription::~Stop_subscription() noexcept = default;

Service_record::Service_record(std::mutex& runtime_mutex) : m_runtime_mutex{runtime_mutex} {}

Service_record::Server_registration Service_record::register_server_connector(
    const Service_interface_identifier& interface,
    SC_impl::Listen_endpoint connector)
{
    // Duplicate server connectors are not allowed.
    SCORE_LANGUAGE_FUTURECPP_ASSERT(!m_server);

    m_server.emplace(Interfaced_server{interface, std::move(connector)});
    auto final_action = [this]() {
        {
            const std::lock_guard<std::mutex> lock{m_runtime_mutex};
            m_server.reset();
        }
    };

    return Server_registration{std::make_unique<Final_action_registration>(Final_action(std::move(final_action))),
                               m_client};
}

Result<Service_record::Client_registration> Service_record::register_client_connector(
    const Service_interface_identifier& interface,
    CC_impl::Server_indication on_server_update)
{
    if (m_client)
    {
        return MakeUnexpected(Construction_error::duplicate_client);
    }
    m_client.emplace(Interfaced_client{interface, std::move(on_server_update)});

    auto remove_from_registry = [this]() {
        const std::lock_guard<std::mutex> lock{m_runtime_mutex};
        m_client.reset();
    };

    return Client_registration{
        std::make_unique<Final_action_registration>(Final_action(std::move(remove_from_registry))), m_server};
}

Result<Client_connector::Uptr> Runtime_impl::make_client_connector(Service_interface_definition configuration,
                                                                   Service_instance instance,
                                                                   Client_connector::Callbacks callbacks) noexcept
{
    return make_client_connector(
        std::move(configuration), instance, std::move(callbacks), Posix_credentials{::getuid(), ::getgid()});
}

Result<Client_connector::Uptr> Runtime_impl::make_client_connector(Service_interface_definition configuration,
                                                                   Service_instance instance,
                                                                   Client_connector::Callbacks callbacks,
                                                                   const Posix_credentials& credentials) noexcept
{
    if (!is_valid(callbacks))
    {
        return MakeUnexpected(Construction_error::callback_missing);
    }

    // check if one is already registered for this service interface and instance and return error
    // if yes
    auto client_connector =
        std::make_unique<CC_impl>(std::move(configuration), instance, std::move(callbacks), credentials);

    auto registration = register_connector(client_connector->get_configuration(),
                                           client_connector->get_service_instance(),
                                           client_connector->make_on_server_update_callback());

    if (!registration)
    {
        return Unexpected{registration.error()};
    }

    client_connector->set_registration(std::move(*registration));

    return {std::move(client_connector)};
}

Result<Disabled_server_connector::Uptr> Runtime_impl::make_server_connector(
    Server_service_interface_definition configuration,
    Service_instance instance,
    Disabled_server_connector::Callbacks callbacks) noexcept
{
    return make_server_connector(
        std::move(configuration), instance, std::move(callbacks), Posix_credentials{::getuid(), ::getgid()});
}

Result<Disabled_server_connector::Uptr> Runtime_impl::make_server_connector(
    Server_service_interface_definition configuration,
    Service_instance instance,
    Disabled_server_connector::Callbacks callbacks,
    const Posix_credentials& credentials) noexcept
{
    const Service_instance_identifier identifier{configuration.get_interface(), instance};

    if (!is_valid(callbacks))
    {
        return MakeUnexpected(Construction_error::callback_missing);
    }

    {
        const std::lock_guard<std::mutex> lock(this->m_service_identifiers->mutex);
        if (!m_service_identifiers->data.insert(identifier).second)
        {
            return MakeUnexpected(Construction_error::duplicate_service);
        }
    }

    Final_action final_action{[identifier, identifiers = std::weak_ptr<Service_identifiers>{m_service_identifiers}]() {
        const auto locked_identifiers = identifiers.lock();
        if (locked_identifiers)
        {
            const std::lock_guard<std::mutex> lock(locked_identifiers->mutex);
            locked_identifiers->data.erase(identifier);
        }
    }};

    return {std::make_unique<SC_impl>(
        *this, std::move(configuration), instance, std::move(callbacks), std::move(final_action), credentials)};
}

Result<Service_bridge_registration> Runtime_impl::register_service_bridge(
    Bridge_identity identity,
    Request_service_function request_service) noexcept
{
    if (!request_service)
    {
        return MakeUnexpected(Construction_error::callback_missing);
    }
    // false positive, registration is moved at return
    // stack allocation not possible as the object needs a stable memory address
    auto registration = std::make_unique<Bridge_registration_handle_impl>(*this, identity);

    const auto create_service_request = [request_service](const auto& interface_configuration) {
        return request_service(std::get<0>(interface_configuration), std::get<1>(interface_configuration));
    };

    std::unique_lock<std::mutex> lock{m_bridge_mutex};
    m_bridge_to_callbacks[registration.get()] = std::make_tuple(std::move(request_service), Interfaces_instances{});

    register_bridge(registration.get(), lock, m_service_requests, create_service_request);

    return Result<Service_bridge_registration>{std::move(registration)};
}

Result<Registration> Runtime_impl::register_connector(const Service_interface_definition& configuration,
                                                      const Service_instance& instance,
                                                      const CC_impl::Server_indication& on_server_update)
{
    std::unique_lock<std::mutex> lock{m_runtime_mutex};
    auto& sii_record = m_database.get_record(configuration.interface, instance);
    auto result = sii_record.register_client_connector(configuration.interface, on_server_update);
    lock.unlock();

    if (!result)
    {
        return Unexpected{result.error()};
    }

    Registration bridged_service_requests;
    if (result->current_server)
    {
        if (is_minor_version_compatible(result->current_server->interface, configuration.interface))
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT(
                is_interface_compatible(result->current_server->interface, configuration.interface));
            on_server_update(result->current_server->endpoint);
        }
        else
        {
            score::mw::log::LogError() << "SOCom error: Bind client to server - minor version incompatible:"
                                       << " client=" << configuration.interface.id
                                       << ", server=" << result->current_server->interface.id
                                       << ", instance=" << instance.id;
        }
    }
    else
    {
        bridged_service_requests = bridge_service_requests(configuration, instance);
    }

    return std::make_unique<Registration_collection>(std::move(bridged_service_requests),
                                                     std::move(result->registration));
}

Registration Runtime_impl::register_connector(const Service_interface_identifier& interface,
                                              const Service_instance& instance,
                                              SC_impl::Listen_endpoint endpoint)
{
    std::unique_lock<std::mutex> lock{m_runtime_mutex};
    auto& sii_record = m_database.get_record(interface, instance);
    auto result = sii_record.register_server_connector(interface, endpoint);
    lock.unlock();

    const auto connect_client = [&endpoint, &interface, &instance](const auto& client) {
        if (is_minor_version_compatible(interface, client.interface))
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT(is_interface_compatible(interface, client.interface));
            client.indication(endpoint);
        }
        else
        {
            score::mw::log::LogError() << "SOCom error: Bind client to server - minor version incompatible:"
                                       << " client=" << client.interface.id << ", server=" << interface.id
                                       << ", instance=" << instance.id;
        }
    };

    if (result.current_client)
    {
        connect_client(*result.current_client);
    }

    return std::move(result.registration);
}

void Runtime_impl::stop_registration(const Bridge_registration_id& id) noexcept
{
    std::unique_lock<std::mutex> bridge_lock{m_bridge_mutex};
    m_bridge_to_callbacks.erase(id);

    cleanup(m_service_requests, id);
    bridge_lock.unlock();
}

Registration Runtime_impl::bridge_service_requests(const Service_interface_definition& configuration,
                                                   const Service_instance& instance)
{
    auto service_requests = get_or_create_service_requests(configuration, instance);

    auto remove_service_requests =
        [this, configuration, instance, service_requests = std::move(service_requests)]() mutable {
            service_requests.reset();
            remove_from_service_requests(configuration, instance);
        };

    Registration service_requests_handle =
        std::make_unique<Final_action_registration>(Final_action(std::move(remove_service_requests)));

    return service_requests_handle;
}

std::shared_ptr<Bridge_id_to_request> Runtime_impl::get_or_create_service_requests(
    const Service_interface_definition& configuration,
    const Service_instance& instance)
{
    const auto create_value = [](const auto& callbacks, const auto& configuration, const auto& instance) {
        return std::get<0>(callbacks.second)(configuration, instance);
    };
    std::unique_lock<std::mutex> bridge_lock{m_bridge_mutex};
    return get_bridge_requests<Bridge_id_to_request>(
        configuration, instance, {}, bridge_lock, m_service_requests, m_bridge_to_callbacks, create_value);
}

void Runtime_impl::remove_from_service_requests(const Service_interface_definition& configuration,
                                                const Service_instance& instance)
{
    const std::lock_guard<std::mutex> bridge_lock{m_bridge_mutex};
    cleanup(m_service_requests, std::make_tuple(configuration, instance));
}

Interfaces_instances Runtime_impl::get_bridge_reported_instances(const Service_interface_identifier& interface,
                                                                 const std::optional<Service_instance>& instance) const
{
    const std::lock_guard<std::mutex> bridge_lock{m_bridge_mutex};
    Instances result;
    for (const auto& bridge_data : m_bridge_to_callbacks)
    {
        auto bridge_services = std::get<1>(bridge_data.second);
        const auto& instances = bridge_services.find(interface);
        if (std::end(bridge_services) != instances)
        {
            static_cast<void>(std::copy_if(std::begin(instances->second),
                                           std::end(instances->second),
                                           std::back_inserter(result),
                                           [&instance](const Service_instance& inst) {
                                               return is_matching_instance(instance, inst);
                                           }));
        }
    }
    return {{interface, result}};
}

}  // namespace score::socom
