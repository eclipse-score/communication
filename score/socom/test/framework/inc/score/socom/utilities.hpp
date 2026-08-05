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

#ifndef SOCOM_UTILITIES_HPP
#define SOCOM_UTILITIES_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <optional>
#include <thread>
#include <vector>

#include "score/socom/payload.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/socom_mocks.hpp"
#include "score/socom/vector_payload.hpp"

namespace score::socom {

/// \brief Vector referencing atomic<bool> objects
using Callbacks_called_t = std::vector<std::reference_wrapper<std::atomic<bool> const>>;

/// \brief Blocks until status becomes true
///
/// Blocks using a loop in which for a short time is slept.
///
/// \param[in] status block until !status returns true
template <typename T>
void wait_for_atomics(T const& status) {
    using namespace std::chrono_literals;
    while (!status) {
        std::this_thread::sleep_for(1ms);
    }
}

/// \brief Blocks until status and stati become true
///
/// Blocks using a loop in which for a short time is slept.
///
/// \param[in] status block until !status returns true
/// \param[in] stati block until all have the value true
template <typename T, typename... Ts>
void wait_for_atomics(T const& status, Ts&&... stati) {
    wait_for_atomics(status);
    wait_for_atomics(stati...);
}

/// \brief Blocks until all references bools in stati become true
///
/// Blocks using a loop in which for a short time is slept.
///
/// \param[in] stati block until all have the value true
void wait_for_atomics_cont(std::vector<std::atomic<bool>> const& stati);

/// \brief Blocks until all references bools in stati become true
///
/// Blocks using a loop in which for a short time is slept.
///
/// \param[in] stati block until all have the value true
void wait_for_atomics_cont(Callbacks_called_t const& stati);

/// \brief Block wait_for_atomics() until counter reached target value
struct Until_equals_value {
    /// \brief Counting variable which is increased by another function
    std::atomic<std::size_t> const& m_counter;
    /// \brief Value to be reached by m_counter
    std::size_t const m_target;
    /// \brief Return true as long m_counter < m_target
    bool operator!() const;
};

/// \brief Returns duration until now since start in seconds as string
///
/// \param[in] start starting time point of the duration
/// \return the duration as seconds with floating pointer number
std::string diff_s(std::chrono::steady_clock::time_point const& start);

/// \brief At destruction print a message to console prefixed with time passed
class Destructor_printor {
    std::chrono::steady_clock::time_point const m_start;
    std::string const m_message;

   public:
    /// \brief Creates objects
    ///
    /// \param[in] start time point which shall be used as start point to calculated passed time
    /// \param[in] message text to be printed
    Destructor_printor(std::chrono::steady_clock::time_point start, std::string message);

    Destructor_printor(Destructor_printor const&) = default;
    Destructor_printor(Destructor_printor&&) = default;

    /// \brief Print passed time and message to console
    ~Destructor_printor();

    Destructor_printor& operator=(Destructor_printor const&) = delete;
    Destructor_printor& operator=(Destructor_printor&&) = delete;
};

/// \brief Holds optional references and has an implicit constructor
template <typename T>
class Optional_reference {
    std::optional<std::reference_wrapper<T>> m_data;

   public:
    /// \brief Creates an object without holding a reference
    Optional_reference() = default;

    /// \brief Creates an object holding a reference to value
    ///
    /// \param[in] value object to be referenced
    // NOLINTNEXTLINE(google-explicit-constructor)
    Optional_reference(T& value) : m_data{value} {}

    /// \return true if a reference is hold, false otherwise
    explicit operator bool() const { return static_cast<bool>(m_data); };

    /// \return the referenced object
    T& operator*() { return m_data->get(); }

    /// \return the referenced object
    const T& operator*() const { return m_data->get(); }
};

/// \brief Appends src to dst by moving data
///
/// \param[in] dst target to which src gets appended onto
/// \param[in] src source of to be appended data
template <typename T>
void append(std::vector<T>& dst, std::vector<T>&& src) {
    dst.insert(std::end(dst), std::make_move_iterator(std::begin(src)),
               std::make_move_iterator(std::end(src)));
}

/// \brief Increase the size of data and will with random bytes if new_size is bigger
///
/// \param[in] data vector with random data
/// \param[in] new_size size to which the vector shall be increased
void increase_and_fill(Vector_buffer& data, std::size_t new_size);

/// \brief Create interfaces without events and methods but with unique names
///
/// \param[in] num number of interfaces to create
/// \return the created interfaces
std::vector<Server_service_interface_definition> create_service_interfaces(std::size_t num);

/// \brief Create interface configuration without events and methods but with name including the
/// interface id
///
/// \param[in] interface_id interface id to identify the service
/// \return the created service interface
Server_service_interface_definition create_service_interface_configuration(size_t interface_id);

/// \brief Create Service_instances with unique names
/// \param[in] num the number of instances to create
/// \return the created instances
std::vector<Service_instance> create_instances(std::size_t num);

/// \brief create one Service_instance with instance_id
///
/// \param[in] instance_id instance id to identify the service
/// \return the created service_instance
Service_instance create_service_instance(size_t instance_id);

/// \brief Matcher that compares a Payload's contents against an expected Payload.
///
/// Clones the expected payload so the matcher owns its own copy.
///
/// \param[in] expected the payload to compare against
/// \return a gmock matcher for Payload const&
inline auto payload_eq(Payload const& expected) {
    auto cloned = std::make_shared<Payload>(clone_payload(expected));
    return ::testing::Truly([cloned](Payload const& p) { return p == *cloned; });
}

/// Set promise after count calls of callback.
///
/// \param[in] num_event_callback_called counter of callback
/// \param[in] count minimum count of callback calls
/// \param[in] event_received promise to fulfill after count calls
/// \return lambda which sets event_received after count calls
inline auto create_check_update_count(std::atomic<std::uint32_t>& num_callback_called,
                                      std::size_t const& count, std::promise<void> event_received) {
    num_callback_called = 0;
    return ::testing::InvokeWithoutArgs(
        [&num_callback_called, count,
         event_received = std::make_shared<std::promise<void>>(std::move(event_received))]() {
            num_callback_called++;
            if (count == num_callback_called) {
                event_received->set_value();
            }
        });
}

}  // namespace score::socom

namespace score {
namespace socom {

// the following streaming operators are needed for fixing valgrind.
// When no operator<< is defined it reads the parameter byte by byte and if there is uninitialized
// data due to alignment valgrind reports an error
template <typename RET, typename... ARGS>
std::ostream& operator<<(std::ostream& out, std::function<RET(ARGS...)> const& /*function*/) {
    return out;
}

std::ostream& operator<<(std::ostream& out, Method_result const& /*unused*/);

std::ostream& operator<<(std::ostream& out, Service_interface_definition const& /*unused*/);

std::ostream& operator<<(std::ostream& out, Server_service_interface_definition const& conf);

std::ostream& operator<<(std::ostream& out, Service_state const& state);

std::ostream& operator<<(std::ostream& out, Construction_error const& error);

bool operator==(Disabled_server_connector const& /*lhs*/, Disabled_server_connector const& /*rhs*/);

}  // namespace socom
}  // namespace score

namespace score {
namespace socom {

bool operator==(Posix_credentials const& lhs, Posix_credentials const& rhs);

}  // namespace socom
}  // namespace score

#endif
