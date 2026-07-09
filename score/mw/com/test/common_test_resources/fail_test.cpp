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

#include "score/mw/com/test/common_test_resources/fail_test.h"

#include "score/assert.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>

namespace score::mw::com::test
{
namespace
{

thread_local std::optional<std::function<void()>> g_fail_test_exit_function{};

}

namespace detail
{

void FailTest(std::stringstream&& strstr)
{
    if (g_fail_test_exit_function.has_value())
    {
        std::invoke(g_fail_test_exit_function.value());
        g_fail_test_exit_function.reset();
    }
    strstr << "\033[0m  \033[1m\033[41mTEST FAILED\033[0m\n";
    std::cout << strstr.str() << std::flush;
    std::_Exit(EXIT_FAILURE);
}

}  // namespace detail

ExitFunctionGuard::ExitFunctionGuard(ExitFunction exit_function)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(!g_fail_test_exit_function.has_value(),
                                            "A test exit function is already set for this thread! Only one "
                                            "ExitFunctionGuard can be created per thread.");
    g_fail_test_exit_function.emplace(std::move(exit_function));
}

ExitFunctionGuard::~ExitFunctionGuard()
{
    if (g_fail_test_exit_function.has_value())
    {
        std::invoke(g_fail_test_exit_function.value());
        g_fail_test_exit_function.reset();
    }
}

void ExitFunctionGuard::Release()
{
    g_fail_test_exit_function.reset();
}

}  // namespace score::mw::com::test
