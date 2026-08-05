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

#include "score/socom/move_only_function_mock.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <score/move_only_function.hpp>
#include <type_traits>

namespace score::socom {

using cpp::move_only_function;
using ::testing::ByMove;
using ::testing::Return;

using Task_function = move_only_function<void()>;
using Value_returning_function = move_only_function<int()>;
using Value_consuming_function = move_only_function<void(int), 128U>;
using Return_value_function =
    move_only_function<std::unique_ptr<int>(int), 128U, alignof(std::max_align_t)>;

TEST(move_only_function_mock_test, void_return_works) {
    Move_only_function_mock<Value_consuming_function> mock;
    EXPECT_CALL(mock, Call(42)).Times(1);
    mock.as_function()(42);
}

TEST(move_only_function_mock_test, value_return_works) {
    Move_only_function_mock<Return_value_function> mock;
    EXPECT_CALL(mock, Call(7)).WillOnce(Return(ByMove(std::make_unique<int>(11))));
    auto const result = mock.as_function()(7);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, 11);
}

void call_callback(Value_consuming_function callback, int const value) { callback(value); }

TEST(move_only_function_mock_test, passed_into_function) {
    Move_only_function_mock<Value_consuming_function> mock;
    EXPECT_CALL(mock, Call(42)).Times(1);
    call_callback(mock.as_function(), 42);
}

TEST(move_only_function_mock_test, no_arguments_no_return_value_works) {
    Move_only_function_mock<Task_function> mock;
    EXPECT_CALL(mock, Call()).Times(1);
    mock.as_function()();
}

TEST(move_only_function_mock_test, no_arguments_with_return_value_works) {
    Move_only_function_mock<Value_returning_function> mock;
    EXPECT_CALL(mock, Call()).Times(1).WillOnce(Return(42));
    EXPECT_EQ(mock.as_function()(), 42);
}

TEST(move_only_function_mock_test, construction_from_signature_works) {
    Move_only_function_mock<int(int)> mock;
    EXPECT_CALL(mock, Call(42)).Times(1).WillOnce(Return(11));
    EXPECT_EQ(11, mock.as_function()(42));
}

TEST(move_only_function_mock_test, construction_from_std_function_works) {
    using Function_t = std::function<int(int)>;
    Move_only_function_mock<Function_t> mock;
    EXPECT_CALL(mock, Call(42)).Times(1).WillOnce(Return(11));
    EXPECT_EQ(11, mock.as_function()(42));
}

}  // namespace score::socom
