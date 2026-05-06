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
    strstr << std::forward<Start>(start);
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

/// \brief Fail a test by exiting the program with EXIT_FAILURE and printing a message to stderr.
/// \param args variadic number of arguments, each one must be streamable to a standard stringstream.
template <typename... Args>
void FailTest(Args&&... args)
{
    std::stringstream strstr;
    strstr << "\033[1m\033[31m";
    detail::FailTest(std::move(strstr), std::forward<Args>(args)...);
}

/// \brief Fail a test if a condition is true, by exiting the program with EXIT_FAILURE and printing a message to
/// stderr.
/// \tparam Args variadic number of argument types, each one must be streamable to a standard stringstream.
/// \param condition if true, the program will exit with EXIT_FAILURE and print the message created from args to stderr.
/// \param args variadic number of arguments, each one must be streamable to a standard stringstream.
template <typename... Args>
void FailTestIf(bool condition, Args&&... args)
{
    if (condition)
    {
        FailTest(std::forward<Args>(args)...);
    }
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_FAIL_TEST_H
