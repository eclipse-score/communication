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

#include "temporary_thread_id_add.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>

namespace score {
namespace socom {

Temporary_thread_id_add::Temporary_thread_id_add(std::mutex& mutex,
                                                 std::vector<std::thread::id>& thread_ids)
    : m_mutex{mutex}, m_thread_ids{thread_ids}, m_id{std::this_thread::get_id()} {
    std::lock_guard<std::mutex> const lock{m_mutex};
    m_thread_ids.emplace_back(m_id);
}

Temporary_thread_id_add::~Temporary_thread_id_add() noexcept {
    std::lock_guard<std::mutex> const lock{m_mutex};
    auto const id = std::find(std::begin(m_thread_ids), std::end(m_thread_ids), m_id);
    assert(std::end(m_thread_ids) != id);
    m_thread_ids.erase(id);
}

Temporary_thread_id_add Deadlock_detector::enter_callback() {
    return Temporary_thread_id_add{m_mutex, m_thread_ids};
}

// Destructors that could cause exceptions are never called because of process termination.
void Deadlock_detector::check_deadlock(
    On_deadlock_detected_callback const& on_deadlock_detected) noexcept {
    std::lock_guard<std::mutex> const lock{m_mutex};
    auto const thread_id =
        std::find(std::begin(m_thread_ids), std::end(m_thread_ids), std::this_thread::get_id());
    if (std::end(m_thread_ids) != thread_id) {
        // destruction from within callback detected
        // death tests cannot contribute to code coverage
        on_deadlock_detected();
        // Here we need to use
        //   std::abort() due to robustness requirements,
        //   ID: AdaptiveCore.ResourceRobustness.FatalSyscall
        // Note: This used to be std::exit(EXIT_FAILURE), but the current DLT implementation
        //  is not fork() compatible. This is explicitly done in unit tests which expect a
        //  fatal log message, e.g. via EXPECT_EXIT().
        std::abort();
    }
}

}  // namespace socom
}  // namespace score
