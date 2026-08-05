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

#include "score/socom/multi_threaded_test_template.hpp"

#include <future>
#include <thread>

#include "score/socom/utilities.hpp"

using namespace std::chrono_literals;

namespace score::socom {

using Thread_function_t =
    std::function<void(Stop_condition, std::atomic<bool>&, std::atomic<bool> const&)>;

Thread_function_t create_thread_function(Loop_function_t const& fun) {
    auto const thread_fun = [&fun](Stop_condition const& stop_condition,
                                   std::atomic<bool>& thread_started,
                                   std::atomic<bool> const& start_loop) {
        thread_started = true;
        while (!start_loop) {
            std::this_thread::yield();
        }
        for (; !stop_condition();) {
            fun();
        }
    };
    return thread_fun;
}

bool Num_iterations::operator()() { return current++ >= max; }

bool Timeout::operator()() const { return std::chrono::steady_clock::now() >= deadline; }

bool And_stop::operator()() const {
    // && is short circuit but both should be always called
    auto const left_result = left();
    auto const right_result = right();
    return left_result && right_result;
}

bool Or_stop::operator()() const {
    // || is short circuit but both should be always called
    auto const left_result = left();
    auto const right_result = right();
    return left_result || right_result;
}

void multi_threaded_test_template(std::vector<Loop_function_t> const& thread_functions,
                                  Stop_condition const& caller_stop_condition) {
    auto const num_iterations = size_t{100};
    auto const default_timeout = 5s;
    auto const stop_condition =
        Or_stop{Timeout{std::chrono::steady_clock::now() + default_timeout},
                And_stop{Num_iterations{num_iterations}, caller_stop_condition}};

    auto threads_started = std::vector<std::atomic<bool>>(thread_functions.size());
    auto results = std::vector<std::future<void>>();
    results.reserve(thread_functions.size());

    std::atomic<bool> start_loops{false};
    auto threads_started_iter = std::begin(threads_started);
    for (auto const& thread_fun : thread_functions) {
        *threads_started_iter = false;
        results.emplace_back(std::async(std::launch::async, create_thread_function(thread_fun),
                                        stop_condition, std::ref(*threads_started_iter),
                                        std::cref(start_loops)));
        threads_started_iter++;
    }

    wait_for_atomics_cont(threads_started);
    start_loops = true;

    // wait for each future
    results.clear();
}

}  // namespace score::socom
