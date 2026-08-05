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

#ifndef SCORE_SOCOM_MOVE_ONLY_FUNCTION_MOCK_HPP
#define SCORE_SOCOM_MOVE_ONLY_FUNCTION_MOCK_HPP

#include <gmock/gmock.h>

#include <cstddef>
#include <functional>
#include <score/move_only_function.hpp>
#include <type_traits>
#include <utility>

namespace score::socom {

namespace internal {

template <typename Function, typename Return_type, typename... Args>
class Move_only_function_mock_base : public ::testing::MockFunction<Return_type(Args...)> {
   public:
    using Base = ::testing::MockFunction<Return_type(Args...)>;

    using Base::Call;

    template <typename Function_t>
    Function_t as_function_impl() {
        return [this](Args... args) -> Return_type {
            if constexpr (std::is_void_v<Return_type>) {
                this->Call(std::forward<Args>(args)...);
            } else {
                return this->Call(std::forward<Args>(args)...);
            }
        };
    }

    std::function<Return_type(Args...)> AsStdFunction() {
        return as_function_impl<std::function<Return_type(Args...)>>();
    }

    Function as_function() { return as_function_impl<Function>(); }
};

}  // namespace internal

template <typename Function>
class Move_only_function_mock;

template <typename Return_type, typename... Args>
class Move_only_function_mock<Return_type(Args...)>
    : public internal::Move_only_function_mock_base<std::function<Return_type(Args...)>,
                                                    Return_type, Args...> {};

template <typename Return_type, typename... Args>
class Move_only_function_mock<std::function<Return_type(Args...)>>
    : public internal::Move_only_function_mock_base<std::function<Return_type(Args...)>,
                                                    Return_type, Args...> {};

template <typename Return_type, typename... Args, std::size_t Storage_size, std::size_t Alignment>
class Move_only_function_mock<
    ::score::cpp::move_only_function<Return_type(Args...), Storage_size, Alignment>>
    : public internal::Move_only_function_mock_base<
          ::score::cpp::move_only_function<Return_type(Args...), Storage_size, Alignment>,
          Return_type, Args...> {};

}  // namespace score::socom

#endif  // SCORE_SOCOM_MOVE_ONLY_FUNCTION_MOCK_HPP
