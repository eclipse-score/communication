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

namespace score::mw::com::test
{

namespace detail
{
void fail_test_(std::stringstream&& strstr);

template <typename Start, typename... Tail>
void fail_test_(std::stringstream&& strstr, Start&& start, Tail&&... tail)
{
    strstr << std::forward<Start>(start);
    if constexpr (sizeof...(Tail) > 0U)
    {
        fail_test_(std::move(strstr), std::forward<Tail>(tail)...);
    }
    else
    {
        fail_test_(std::move(strstr));
    }
}
}  // namespace detail

template <typename... Args>
void fail_test(Args&&... args)
{
    std::stringstream strstr;
    strstr << "\033[1m\033[31m";
    detail::fail_test_(std::move(strstr), std::forward<Args>(args)...);
}

template <typename... Args>
void fail_test_if(bool condition, Args&&... args)
{
    if (condition)
    {
        fail_test(std::forward<Args>(args)...);
    }
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_FAIL_TEST_H
