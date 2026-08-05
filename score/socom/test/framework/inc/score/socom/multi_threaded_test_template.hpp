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

#ifndef MULTI_THREADED_TEST_TEMPLATE_HPP
#define MULTI_THREADED_TEST_TEMPLATE_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <vector>

namespace score::socom {

/// \brief Function is called in a tight loop
using Loop_function_t = std::function<void()>;
/// \brief When true is returned the loop will be terminated
using Stop_condition = std::function<bool()>;

/// \brief Stop thread execution after maximum number of checks is reached
struct Num_iterations {
    size_t max{0};
    size_t current{0};
    bool operator()();
};

/// \brief Stop thread execution after timeout
struct Timeout {
    std::chrono::steady_clock::time_point deadline;
    bool operator()() const;
};

/// \brief Stop thread execution when both stop conditions are met
struct And_stop {
    Stop_condition left;
    Stop_condition right;
    bool operator()() const;
};

/// \brief Stop thread execution when one stop conditions is met
struct Or_stop {
    Stop_condition left;
    Stop_condition right;
    bool operator()() const;
};

/// \brief Execute each given loop function a thread until caller_stop_condition is true
///
/// All started threads are synced after creation so that thread creation has no timing
/// impact on which function in thread_functions is called first. Ideally all functions
/// are called first at the same time.
///
/// \param[in] thread_functions each function is executed endlessly in a separate thread
/// \param[in] caller_stop_condition when true threads are stopped
void multi_threaded_test_template(std::vector<Loop_function_t> const& thread_functions,
                                  Stop_condition const& caller_stop_condition);

}  // namespace score::socom

#endif  // MULTI_THREADED_TEST_TEMPLATE_HPP
