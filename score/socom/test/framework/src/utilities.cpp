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

#include "score/socom/utilities.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <type_traits>

#include "gtest/gtest.h"
#include "score/socom/client_connector.hpp"
#include "score/socom/error.hpp"
#include "score/socom/method.hpp"
#include "score/socom/payload.hpp"

namespace score::socom {

void wait_for_atomics_cont(std::vector<std::atomic<bool>> const& stati) {
    for (auto const& status : stati) {
        wait_for_atomics(status);
    }
}

void wait_for_atomics_cont(Callbacks_called_t const& stati) {
    for (auto const& status : stati) {
        wait_for_atomics(status.get());
    }
}

bool Until_equals_value::operator!() const { return m_counter < m_target; }

std::string diff_s(std::chrono::steady_clock::time_point const& start) {
    using float_seconds = std::chrono::duration<float>;
    auto const diff =
        std::chrono::duration_cast<float_seconds>(std::chrono::steady_clock::now() - start).count();
    return std::to_string(diff) + "s";
}

Destructor_printor::Destructor_printor(std::chrono::steady_clock::time_point start,
                                       std::string message)
    : m_start{start}, m_message{std::move(message)} {}

Destructor_printor::~Destructor_printor() {
    std::cout << diff_s(m_start) << " " << m_message << std::endl;
}

void increase_and_fill(Vector_buffer& data, std::size_t const new_size) {
    auto const old_size = data.size();
    if (new_size <= old_size) {
        return;
    }
    data.resize(new_size);

    std::random_device rd;
    std::mt19937 gen(rd());

    using int_t = std::underlying_type_t<Payload::Byte>;

    std::uniform_int_distribution<> distrib(std::numeric_limits<int_t>::min(),
                                            std::numeric_limits<int_t>::max());
    auto start = std::begin(data);
    std::advance(start, old_size);

    std::generate(start, std::end(data), [&gen, &distrib]() { return std::byte(distrib(gen)); });
}

Service_interface_identifier create_service_interface(size_t const interface_id) {
    return Service_interface_identifier{std::string{"interface_" + std::to_string(interface_id)},
                                        {4, 2}};
}

Server_service_interface_definition create_service_interface_configuration(
    size_t const interface_id) {
    return Server_service_interface_definition{create_service_interface(interface_id), {}, {}};
}

std::vector<Server_service_interface_definition> create_service_interfaces(size_t const num) {
    std::vector<Server_service_interface_definition> interfaces;
    interfaces.reserve(num);
    for (size_t i = 0; i < num; i++) {
        interfaces.emplace_back(create_service_interface_configuration(i));
    }
    return interfaces;
}

Service_instance create_service_instance(size_t instance_id) {
    return Service_instance{std::string{"instance_" + std::to_string(instance_id)}};
}

std::vector<Service_instance> create_instances(size_t const num) {
    std::vector<Service_instance> instances;
    instances.reserve(num);
    for (size_t i = 0; i < num; i++) {
        instances.emplace_back(create_service_instance(i));
    }
    return instances;
}

}  // namespace score::socom

namespace score {
namespace socom {

std::ostream& operator<<(std::ostream& out, Method_result const& /*method_result*/) { return out; }

std::ostream& operator<<(std::ostream& out,
                         Service_interface_definition const& /*service_interface_configuration*/) {
    return out;
}

std::ostream& operator<<(std::ostream& out, Server_service_interface_definition const& conf) {
    return operator<<(out, static_cast<Service_interface_definition>(conf));
}

std::ostream& operator<<(std::ostream& out, Service_state const& state) {
    std::map<Service_state, std::string> state_to_string{
        {Service_state::available, "Service_state::available"},
        {Service_state::not_available, "Service_state::not_available"}};
    out << state_to_string.at(state);
    return out;
}

std::ostream& operator<<(std::ostream& out, Construction_error const& error) {
    std::map<Construction_error, std::string> error_to_string{
        {Construction_error::callback_missing, "Construction_error::callback_missing"},
        {Construction_error::duplicate_service, "Construction_error::duplicate_service"}};
    out << error_to_string.at(error);
    return out;
}

bool operator==(Disabled_server_connector const& /*lhs*/,
                Disabled_server_connector const& /*rhs*/) {
    ADD_FAILURE();
    return false;
}

}  // namespace socom
}  // namespace score

namespace score {
namespace socom {

bool operator==(Posix_credentials const& lhs, Posix_credentials const& rhs) {
    return (lhs.uid == rhs.uid) && (lhs.gid == rhs.gid);
}

}  // namespace socom
}  // namespace score
