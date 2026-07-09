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
#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_FAIL_TEST_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_FAIL_TEST_H

#include "score/result/error.h"

#include <functional>
#include <sstream>
#include <utility>

namespace score::mw::com::test
{

namespace detail
{

void FailTest(std::stringstream&& strstr);

template <typename Start, typename... Tail>
void FailTest(std::stringstream&& strstr, Start&& start, Tail&&... tail)
{
    // Since score::result::Error does not have an operator<< overload for a stringstream and no method to convert it to
    // a string, we manually stringify it here.
    if constexpr (std::is_same_v<std::decay_t<Start>, score::result::Error>)
    {
        strstr << start.Message() << " / " << start.UserMessage();
    }
    else
    {
        strstr << std::forward<Start>(start);
    }

    if constexpr (sizeof...(Tail) > 0U)
    {
        FailTest(std::move(strstr), std::forward<Tail>(tail)...);
    }
    else
    {
        FailTest(std::move(strstr));
    }
}

}  // namespace detail

/// \brief RAII guard to set a test exit function that will be called when FailTest is invoked or when the guard is
/// destroyed.
class ExitFunctionGuard
{
  public:
    using ExitFunction = std::function<void()>;

    explicit ExitFunctionGuard(ExitFunction exit_function);
    ~ExitFunctionGuard();

    static void Release();

    ExitFunctionGuard(const ExitFunctionGuard&) = delete;
    ExitFunctionGuard& operator=(const ExitFunctionGuard&) = delete;
    ExitFunctionGuard(ExitFunctionGuard&&) = delete;
    ExitFunctionGuard& operator=(ExitFunctionGuard&&) = delete;
};

/// \brief Fail a test by exiting the program with EXIT_FAILURE and printing a message to stderr.
/// \param args variadic number of arguments, each one must be streamable to a standard stringstream.
template <typename... Args>
void FailTest(Args&&... args)
{
    std::stringstream strstr;
    strstr << "\033[1m\033[31m";
    detail::FailTest(std::move(strstr), std::forward<Args>(args)...);
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_FAIL_TEST_H
