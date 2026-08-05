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

#ifndef SRC_SOCOM_INCLUDE_SCORE_SOCOM_SERVICE_INTERFACE_IDENTIFIER
#define SRC_SOCOM_INCLUDE_SCORE_SOCOM_SERVICE_INTERFACE_IDENTIFIER

#include <cstdint>
#include <functional>
#include <string>
#include <tuple>

#include "score/socom/registry_string_view.hpp"
#include "score/socom/string_registry.hpp"

namespace score::socom {

/// Service instance identification information
class Service_instance final {
   public:
    using Id = Registry_string_view;

    /// String-based service instance identifier.
    Id id;

    /// \brief Constructor.
    /// \param new_id ID of the service interface.
    explicit Service_instance(Id new_id) noexcept : id{new_id} {}

    /// \brief Constructor.
    /// \param new_id ID of the service interface.
    explicit Service_instance(std::string_view new_id)
        : id{score::socom::instance_id_registry().insert(new_id).first} {}

    /// \brief Constructor.
    /// \param new_id ID of the service interface.
    /// \param is_static_string_literal Tag to indicate that the provided string is a static string
    ///                                 literal.
    Service_instance(std::string_view new_id, Literal_tag is_static_string_literal)
        : id{score::socom::instance_id_registry().insert(new_id, is_static_string_literal).first} {}

    /// \brief Constructor.
    /// \param new_id ID of the service interface.
    explicit Service_instance(std::string&& new_id)
        : id{score::socom::instance_id_registry().insert(std::move(new_id)).first} {}
};

/// \brief Operator == for Service_instance.
/// \param lhs Left-hand side of operator.
/// \param rhs Right-hand side of operator.
/// \return True in case of equality, otherwise false.
inline bool operator==(Service_instance const& lhs, Service_instance const& rhs) {
    return lhs.id == rhs.id;
}

/// \brief Operator < for Service_instance.
/// \param lhs Left-hand side of operator.
/// \param rhs Right-hand side of operator.
/// \return True in case of lhs is less than rhs, otherwise false.
inline bool operator<(Service_instance const& lhs, Service_instance const& rhs) {
    return lhs.id < rhs.id;
}

/// \brief Service interface identification information.
struct Service_interface_identifier {
   public:
    /// \brief Alias for a service interface identifier.
    using Id = Registry_string_view;

    /// \brief Service interface version type.
    struct Version {
        /// \brief Major version information.
        /// \note Major version must match exactly for service interface compatibility.
        std::uint16_t major;
        /// \brief Minor version information.
        /// \note Minor version of Client_connector is less or equal than the minor version of
        /// Server_connector for service interface compatibility.
        std::uint16_t minor;
    };

    /// \brief Service interface identifier.
    Id id;

    /// \brief Service interface version information.
    Version version;

    /// \brief Constructor.
    /// \param new_id ID of the service interface.
    /// \param new_version Version of the service interface.
    Service_interface_identifier(Id new_id, Version new_version) noexcept
        : id{new_id}, version{new_version} {}

    /// \brief Constructor.
    /// \param new_id ID of the service interface.
    /// \param new_version Version of the service interface.
    Service_interface_identifier(std::string_view new_id, Version new_version)
        : id{score::socom::service_id_registry().insert(new_id).first}, version{new_version} {}

    /// \brief Constructor.
    /// \param new_id ID of the service interface.
    /// \param is_static_string_literal Tag to indicate that the provided string is a static string
    ///                                 literal.
    /// \param new_version Version of the service interface.
    Service_interface_identifier(std::string_view new_id, Literal_tag is_static_string_literal,
                                 Version new_version)
        : id{score::socom::service_id_registry().insert(new_id, is_static_string_literal).first},
          version{new_version} {}

    /// \brief Constructor.
    /// \param new_id ID of the service interface.
    /// \param new_version Version of the service interface.
    Service_interface_identifier(std::string&& new_id, Version new_version)
        : id{score::socom::service_id_registry().insert(std::move(new_id)).first},
          version{new_version} {}
};

/// \brief Operator == for Service_interface_identifier::Version.
/// \param lhs Left-hand side of operator.
/// \param rhs Right-hand side of operator.
/// \return True in case of equality, otherwise false.
inline bool operator==(Service_interface_identifier::Version const& lhs,
                       Service_interface_identifier::Version const& rhs) {
    return (std::tie(lhs.major, lhs.minor) == std::tie(rhs.major, rhs.minor));
}

/// \brief Operator < for Service_interface_identifier::Version.
/// \param lhs Left-hand side of operator.
/// \param rhs Right-hand side of operator.
/// \return True in case the contents of lhs are lexicographically less than the contents of rhs,
/// otherwise false.
inline bool operator<(Service_interface_identifier::Version const& lhs,
                      Service_interface_identifier::Version const& rhs) {
    return (std::tie(lhs.major, lhs.minor) < std::tie(rhs.major, rhs.minor));
}

/// \brief Operator == for Service_interface_identifier.
/// \param lhs Left-hand side of operator.
/// \param rhs Right-hand side of operator.
/// \return True in case of equality, otherwise false.
inline bool operator==(Service_interface_identifier const& lhs,
                       Service_interface_identifier const& rhs) {
    return (std::tie(lhs.id, lhs.version) == std::tie(rhs.id, rhs.version));
}

/// \brief Operator < for Service_interface_identifier.
/// \param lhs Left-hand side of operator.
/// \param rhs Right-hand side of operator.
/// \return True in case the contents of lhs are lexicographically less than the contents of rhs,
/// otherwise false.
inline bool operator<(Service_interface_identifier const& lhs,
                      Service_interface_identifier const& rhs) {
    return (std::tie(lhs.id, lhs.version) < std::tie(rhs.id, rhs.version));
}

}  // namespace score::socom

/// \brief std::hash specialization for Service_instance
///
/// \return Hash value for the given Service_instance
///
template <>
struct std::hash<score::socom::Service_instance> {
    std::size_t operator()(score::socom::Service_instance const& s) const noexcept {
        return std::hash<score::socom::Registry_string_view>{}(s.id);
    }
};

/// \brief std::hash specialization for Service_interface_identifier
///
/// \return Hash value for the given Service_interface_identifier
///
template <>
struct std::hash<score::socom::Service_interface_identifier> {
    std::size_t operator()(score::socom::Service_interface_identifier const& s) const noexcept {
        std::size_t const h1 = std::hash<score::socom::Registry_string_view>{}(s.id);
        std::size_t const h2 = std::hash<std::uint16_t>{}(s.version.major);
        std::size_t const h3 = std::hash<std::uint16_t>{}(s.version.minor);
        auto const hash = h1 ^ (h2 << 1) ^ (h3 << 2);
        return hash;
    }
};

#endif  // SRC_SOCOM_INCLUDE_SCORE_SOCOM_SERVICE_INTERFACE_IDENTIFIER
