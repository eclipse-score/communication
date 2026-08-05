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

#ifndef SRC_SOCOM_INCLUDE_SCORE_SOCOM_SERVICE_INTERFACE_DEFINITION
#define SRC_SOCOM_INCLUDE_SCORE_SOCOM_SERVICE_INTERFACE_DEFINITION

#include <cstdint>

#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/service_interface_identifier.hpp"

namespace score::socom {

/// description: Strong type for forced proper construction.
enum class Num_of_events : std::uint16_t {};

/// description: Strong type for forced proper construction.
enum class Num_of_methods : std::uint16_t {};

inline Num_of_events to_num_of_events(std::size_t const value) noexcept {
    return static_cast<Num_of_events>(value);
}

inline Num_of_methods to_num_of_methods(std::size_t const value) noexcept {
    return static_cast<Num_of_methods>(value);
}

/// \brief Service interface configuration data structure for Client_connector instances.
/// \details This type, which is used by Runtime::make_client_connector(), allows an optional member
/// configuration.
struct Service_interface_definition final {
    /// \brief Constructor for default use-case.
    /// \param sif Service interface identification information.
    /// \param methods Methods of the service interface.
    /// \param events Events of the service interface.
    Service_interface_definition(Service_interface_identifier sif, Num_of_methods num_of_methods,
                                 Num_of_events num_of_events);

    /// \brief Constructor without methods and events.
    /// \details Client_connectors which have no member configuration must use the provided
    /// Server_service_interface_definition configuration.
    /// \param sif Service interface identification information.
    explicit Service_interface_definition(Service_interface_identifier sif);

    Service_interface_definition(Service_interface_definition const&) = default;
    Service_interface_definition(Service_interface_definition&&) noexcept = default;

    ~Service_interface_definition() noexcept = default;

    Service_interface_definition& operator=(Service_interface_definition const&) = delete;
    Service_interface_definition& operator=(Service_interface_definition&&) = delete;

    /// \brief Service interface identification information.
    Service_interface_identifier const interface;
    std::uint16_t num_methods{0U};
    std::uint16_t num_events{0U};
};

bool operator==(Service_interface_definition const& lhs, Service_interface_definition const& rhs);

bool operator<(Service_interface_definition const& lhs, Service_interface_definition const& rhs);

/// \brief Service interface configuration data structure for Server_connector instances.
/// \details This type, which is used by Runtime::make_server_connector(), enforces a member
/// configuration.
class Server_service_interface_definition final {
    Service_interface_definition m_configuration;

   public:
    /// \brief Constructor.
    /// \param sif Service interface identification information.
    /// \param methods Methods of the service interface.
    /// \param events Events of the service interface.
    Server_service_interface_definition(Service_interface_identifier const& sif,
                                        Num_of_methods num_of_methods, Num_of_events num_of_events);

    Server_service_interface_definition(Server_service_interface_definition const& rhs);
    Server_service_interface_definition(Server_service_interface_definition&& rhs) noexcept;

    ~Server_service_interface_definition() noexcept = default;

    Server_service_interface_definition& operator=(Server_service_interface_definition const&) =
        delete;
    Server_service_interface_definition& operator=(Server_service_interface_definition&&) = delete;

    // Service_interface_definition
    /// \brief Retrieves the configuration by implicitly converting an instance to
    /// Service_interface_definition.
    /// \return The stored configuration.
    operator Service_interface_definition() const;

    std::uint16_t get_num_methods() const noexcept;
    std::uint16_t get_num_events() const noexcept;
    Service_interface_identifier const& get_interface() const noexcept;
};

}  // namespace score::socom

#endif  // SRC_SOCOM_INCLUDE_SCORE_SOCOM_SERVICE_INTERFACE_DEFINITION
