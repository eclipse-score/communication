/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

#include <gtest/gtest.h>

#include <atomic>
#include <stdexcept>

#include "score/socom/final_action.hpp"

namespace score::socom {

TEST(final_action_test, destructor_executes_functor_once) {
    std::atomic<int> call_count{0};

    {
        Final_action final_action{[&call_count]() { ++call_count; }};
        EXPECT_EQ(call_count.load(), 0);
    }

    EXPECT_EQ(call_count.load(), 1);
}

TEST(final_action_test, execute_runs_functor_and_disarms) {
    std::atomic<int> call_count{0};

    {
        Final_action final_action{[&call_count]() { ++call_count; }};

        final_action.execute();
        EXPECT_EQ(call_count.load(), 1);

        final_action.execute();
        EXPECT_EQ(call_count.load(), 1);
    }

    EXPECT_EQ(call_count.load(), 1);
}

TEST(final_action_test, move_constructor_transfers_execution_and_disarms_source) {
    std::atomic<int> call_count{0};

    {
        Final_action source{[&call_count]() { ++call_count; }};
        Final_action moved{std::move(source)};

        EXPECT_EQ(call_count.load(), 0);
    }

    EXPECT_EQ(call_count.load(), 1);
}

TEST(final_action_test, thrown_exception_is_swallowed_and_disarms) {
    std::atomic<int> call_count{0};

    Final_action final_action{[&call_count]() {
        ++call_count;
        throw std::runtime_error{"boom"};
    }};

    EXPECT_NO_THROW(final_action.execute());
    EXPECT_EQ(call_count.load(), 1);

    EXPECT_NO_THROW(final_action.execute());
    EXPECT_EQ(call_count.load(), 1);
}

}  // namespace score::socom
