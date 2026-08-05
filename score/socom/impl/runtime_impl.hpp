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

#ifndef SCORE_SOCOM_RUNTIME_IMPL_HPP
#define SCORE_SOCOM_RUNTIME_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include "client_connector_impl.hpp"
#include "runtime_registration.hpp"
#include "score/socom/runtime.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "server_connector_impl.hpp"
#include "service_identifier.hpp"

namespace score {
namespace socom {

using CC_impl = client_connector::Impl;
using SC_impl = server_connector::Impl;

using Bridge_registration_id = Service_bridge_registration_handle const*;

template <typename VALUE>
using Bridge_id_to = std::map<Bridge_registration_id, VALUE>;

using Bridge_id_to_request = Bridge_id_to<Service_request>;

template <typename INSTANCE, typename HANDLE>
using Active_bridge_requests = std::map<
    std::tuple<Service_interface_definition, INSTANCE>,
    std::tuple<std::weak_ptr<Bridge_id_to<HANDLE>>, std::vector<std::optional<Bridge_identity>>>>;

template <typename T>
struct Mutexed_variable {
    std::mutex mutex;
    T data{};
};

using Service_identifiers = Mutexed_variable<std::set<Service_instance_identifier>>;

class Service_record {
   public:
    struct Interfaced_server {
        Service_interface_identifier interface;
        SC_impl::Listen_endpoint endpoint;
    };

    struct Interfaced_client {
        Service_interface_identifier interface;
        CC_impl::Server_indication indication;
    };

    using Server = std::optional<Interfaced_server>;
    using Client = std::optional<Interfaced_client>;

    struct Server_registration {
        Registration registration;
        Client current_client;
    };

    struct Client_registration {
        Registration registration;
        Server current_server;
    };

    explicit Service_record(std::mutex& runtime_mutex);

    Server_registration register_server_connector(Service_interface_identifier const& interface,
                                                  SC_impl::Listen_endpoint connector);

    Result<Client_registration> register_client_connector(
        Service_interface_identifier const& interface, CC_impl::Server_indication on_server_update);

    bool is_available() const { return m_server.has_value(); }

   private:
    std::mutex& m_runtime_mutex;
    Server m_server;
    Client m_client;
};

using Instances = std::vector<Service_instance>;
using Interfaces_instances = std::unordered_map<Service_interface_identifier, Instances>;

class Service_database {
   public:
    explicit Service_database(std::mutex& runtime_mutex);

    Service_record& get_record(Service_interface_identifier const& interface,
                               Service_instance const& instance);

    Interfaces_instances get_instances(Service_interface_identifier const& interface,
                                       std::optional<Service_instance> const& filter) const;

    Interfaces_instances get_instances(std::optional<Service_interface_identifier> const& interface,
                                       std::optional<Service_instance> const& filter) const;

   private:
    struct Minor_version_ignoring_key_equal {
        bool operator()(Service_interface_identifier const& lhs,
                        Service_interface_identifier const& rhs) const noexcept {
            return (lhs.id == rhs.id) && (lhs.version.major == rhs.version.major);
        }
    };

    struct Minor_version_ignoring_hash {
        std::size_t operator()(Service_interface_identifier const& sii) const noexcept {
            auto const id_hash = std::hash<Registry_string_view>{}(sii.id);
            auto const major_hash = std::hash<std::uint16_t>{}(sii.version.major);
            return id_hash ^ (major_hash << 1);
        }
    };

    using Service_instances = std::unordered_map<Service_instance, Service_record>;
    using Service_interfaces =
        std::unordered_map<Service_interface_identifier, Service_instances,
                           Minor_version_ignoring_hash, Minor_version_ignoring_key_equal>;

    std::mutex& m_runtime_mutex;
    Service_interfaces m_service_records;
};

struct Stop_subscription {
    Stop_subscription() = default;
    Stop_subscription(Stop_subscription const&) = delete;
    Stop_subscription(Stop_subscription&&) = delete;
    virtual ~Stop_subscription() noexcept;
    Stop_subscription& operator=(Stop_subscription const&) = delete;
    Stop_subscription& operator=(Stop_subscription&&) = delete;

    virtual void stop_registration(Bridge_registration_id const& id) noexcept = 0;
};

class Bridge_registration_handle_impl final : public Service_bridge_registration_handle {
   public:
    explicit Bridge_registration_handle_impl(Stop_subscription& stopper, Bridge_identity identity)
        : m_stopper{stopper}, m_identity{identity} {}
    Bridge_registration_handle_impl(Bridge_registration_handle_impl const&) = delete;
    Bridge_registration_handle_impl(Bridge_registration_handle_impl&&) = delete;
    Bridge_registration_handle_impl& operator=(Bridge_registration_handle_impl const&) = delete;
    Bridge_registration_handle_impl& operator=(Bridge_registration_handle_impl&&) = delete;
    ~Bridge_registration_handle_impl() noexcept override { m_stopper.stop_registration(this); };
    Bridge_identity get_identity() const override { return m_identity; }

   private:
    Stop_subscription& m_stopper;
    Bridge_identity m_identity;
};

// both base classes delete the operator= in question
class Runtime_impl final : public Runtime, public Stop_subscription {
   public:
    using Bridge_registration_to_callbacks =
        Bridge_id_to<std::tuple<Request_service_function, Interfaces_instances>>;

    Result<Client_connector::Uptr> make_client_connector(
        Service_interface_definition configuration, Service_instance instance,
        Client_connector::Callbacks callbacks) noexcept override;

    Result<Client_connector::Uptr> make_client_connector(
        Service_interface_definition configuration, Service_instance instance,
        Client_connector::Callbacks callbacks,
        Posix_credentials const& credentials) noexcept override;

    Result<Disabled_server_connector::Uptr> make_server_connector(
        Server_service_interface_definition configuration, Service_instance instance,
        Disabled_server_connector::Callbacks callbacks) noexcept override;

    Result<Disabled_server_connector::Uptr> make_server_connector(
        Server_service_interface_definition configuration, Service_instance instance,
        Disabled_server_connector::Callbacks callbacks,
        Posix_credentials const& credentials) noexcept override;

    // NOLINTBEGIN(bugprone-exception-escape)(ClangTidy Android Warning)
    Result<Service_bridge_registration> register_service_bridge(
        Bridge_identity identity, Request_service_function request_service) noexcept override;
    // NOLINTEND(bugprone-exception-escape)

    Result<Registration> register_connector(Service_interface_definition const& configuration,
                                            Service_instance const& instance,
                                            CC_impl::Server_indication const& on_server_update);

    Registration register_connector(Service_interface_identifier const& interface,
                                    Service_instance const& instance,
                                    SC_impl::Listen_endpoint endpoint);

    void stop_registration(Bridge_registration_id const& id) noexcept override;

   private:
    std::shared_ptr<Bridge_id_to_request> get_or_create_service_requests(
        Service_interface_definition const& configuration, Service_instance const& instance);

    void remove_from_service_requests(Service_interface_definition const& configuration,
                                      Service_instance const& instance);

    Registration bridge_service_requests(Service_interface_definition const& configuration,
                                         Service_instance const& instance);

    Interfaces_instances get_bridge_reported_instances(
        Service_interface_identifier const& interface,
        std::optional<Service_instance> const& instance) const;

    mutable std::mutex m_runtime_mutex{};
    Service_database m_database{m_runtime_mutex};

    mutable std::mutex m_bridge_mutex;
    Bridge_registration_to_callbacks m_bridge_to_callbacks{};

    Active_bridge_requests<Service_instance, Service_request> m_service_requests{};

    std::shared_ptr<Service_identifiers> m_service_identifiers{
        std::make_shared<Service_identifiers>()};
};

}  // namespace socom
}  // namespace score

#endif  // SCORE_SOCOM_RUNTIME_IMPL_HPP
