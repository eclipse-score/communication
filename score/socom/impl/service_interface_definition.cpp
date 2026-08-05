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

#include "score/socom/service_interface_definition.hpp"

#include <cassert>

namespace score::socom {

Service_interface_definition::Service_interface_definition(Service_interface_identifier const sif,
                                                           Num_of_methods const num_of_methods,
                                                           Num_of_events const num_of_events)
    : interface{sif},
      num_methods{static_cast<std::uint16_t>(num_of_methods)},
      num_events{static_cast<std::uint16_t>(num_of_events)} {}

Service_interface_definition::Service_interface_definition(Service_interface_identifier const sif)
    : interface{sif} {}

bool operator==(Service_interface_definition const& lhs, Service_interface_definition const& rhs) {
    auto const is_equal = !(lhs < rhs) && !(rhs < lhs);
    assert(!is_equal || (lhs.num_events == rhs.num_events && lhs.num_methods == rhs.num_methods));
    return is_equal;
}

bool operator<(Service_interface_definition const& lhs, Service_interface_definition const& rhs) {
    return lhs.interface < rhs.interface;
}

Server_service_interface_definition::Server_service_interface_definition(
    Service_interface_identifier const& sif, Num_of_methods const num_of_methods,
    Num_of_events const num_of_events)
    : m_configuration{sif, num_of_methods, num_of_events} {}

Server_service_interface_definition::Server_service_interface_definition(
    Server_service_interface_definition const& rhs) = default;

Server_service_interface_definition::Server_service_interface_definition(
    Server_service_interface_definition&& rhs) noexcept
    : m_configuration{std::move(rhs.m_configuration)} {}

Server_service_interface_definition::operator Service_interface_definition() const {
    return m_configuration;
}

std::uint16_t Server_service_interface_definition::get_num_methods() const noexcept {
    return m_configuration.num_methods;
}

std::uint16_t Server_service_interface_definition::get_num_events() const noexcept {
    return m_configuration.num_events;
}

Service_interface_identifier const& Server_service_interface_definition::get_interface()
    const noexcept {
    return m_configuration.interface;
}

}  // namespace score::socom
