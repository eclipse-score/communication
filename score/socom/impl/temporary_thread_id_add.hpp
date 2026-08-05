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

#ifndef SCORE_SOCOM_TEMPORARY_THREAD_ID_ADD_HPP
#define SCORE_SOCOM_TEMPORARY_THREAD_ID_ADD_HPP

#include <mutex>
#include <score/move_only_function.hpp>
#include <thread>
#include <vector>

namespace score {
namespace socom {

/// Temporary_thread_id_add adds the current thread::id to the list upon construction and removes it
/// at destruction.
///
/// Before calling a callback of the Client_connector or Enabled_server_connector an instance of
/// this class has to be created. When the callback has returned the instance has to be destroyed.
/// This helps to detect deadlocks in the destructor, which would wait until all callbacks have
/// returned. But when the destructor is triggered from within a callback they wait for each other.
class Temporary_thread_id_add {
    std::mutex& m_mutex;
    std::vector<std::thread::id>& m_thread_ids;
    std::thread::id m_id;

   public:
    /// Add the current thread::id to thread_ids.
    ///
    /// \param[in] mutex thread_ids associated mutex to sync reads and writes
    /// \param[in] thread_ids the thread::id vector of a connector
    Temporary_thread_id_add(std::mutex& mutex, std::vector<std::thread::id>& thread_ids);

    Temporary_thread_id_add(Temporary_thread_id_add const& /*rhs*/) = delete;

    // Move constructor is only declared, but not defined.
    // Without a declared move constructor Temporary_thread_id_add cannot be returned by a function.
    // But thanks to rvalue optimization the compiler does not use the move constructor, thus it is
    // fine to skip the definition. Whenever a move constructor needs to be defined = default is
    // not enough, as we then would run in danger of removing the same thread::id twice.
    Temporary_thread_id_add(Temporary_thread_id_add&& /*rhs*/) noexcept;

    /// Remove the current thread::id from thread_ids.
    ~Temporary_thread_id_add() noexcept;

    Temporary_thread_id_add& operator=(Temporary_thread_id_add const& /*rhs*/) = delete;
    Temporary_thread_id_add& operator=(Temporary_thread_id_add&& /*rhs*/) = delete;
};

/// Helps to detect deadlocks.
///
/// Before calling a callback the function enter_callback() needs to be called and the returned
/// object saved on the stack. After the callback has returned the saved Temporary_thread_id_add
/// object can be destroyed. When the using objects destructor is called check_deadlock() has to be
/// called by the destructor. check_deadlock() checks if any callback is still alive in the
/// callback, which would result in a deadlock, when the destructor waits for the callback to
/// return.
class Deadlock_detector {
    std::mutex m_mutex;
    std::vector<std::thread::id> m_thread_ids;

   public:
    using On_deadlock_detected_callback = cpp::move_only_function<void()>;

    /// Save current thread id until the returned object is destroyed.
    ///
    /// \return RAII object which removes the saved thread id automatically
    Temporary_thread_id_add enter_callback();

    /// Check for a deadlock and terminate upon detection.
    ///
    /// \param[in] on_deadlock_detected function to call before terminating the process in presence
    /// of a deadlock
    void check_deadlock(On_deadlock_detected_callback const& on_deadlock_detected) noexcept;
};

}  // namespace socom
}  // namespace score

#endif  // SCORE_SOCOM_TEMPORARY_THREAD_ID_ADD_HPP
