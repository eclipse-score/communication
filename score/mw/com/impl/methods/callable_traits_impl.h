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
#ifndef SCORE_MW_COM_IMPL_METHODS_METHOD_CALLABLE_TRAITS_IMPL_H
#define SCORE_MW_COM_IMPL_METHODS_METHOD_CALLABLE_TRAITS_IMPL_H

#include <tuple>
#include <type_traits>

namespace score::mw::com::impl
{

template <typename C, typename Ret, typename... InArgs>
struct get_callable_args_types<Ret (C::*)(InArgs...) const noexcept>
{
    using type = std::tuple<InArgs...>;
};

template <typename C, typename Ret, typename... InArgs>
struct get_callable_args_types<Ret (C::*)(InArgs...) const>
{
    using type = std::tuple<InArgs...>;
};

template <typename C, typename Ret, typename... InArgs>
struct get_callable_args_types<Ret (C::*)(InArgs...) noexcept>
{
    using type = std::tuple<InArgs...>;
};

template <typename C, typename Ret, typename... InArgs>
struct get_callable_args_types<Ret (C::*)(InArgs...)>
{
    using type = std::tuple<InArgs...>;
};

template <typename C, typename Ret, typename... Args>
struct get_callable_return_type<Ret (C::*)(Args...) const>
{
    using type = Ret;
};

template <typename C, typename Ret, typename... Args>
struct get_callable_return_type<Ret (C::*)(Args...) const noexcept>
{
    using type = Ret;
};

template <typename C, typename Ret, typename... Args>
struct get_callable_return_type<Ret (C::*)(Args...)>
{
    using type = Ret;
};

template <typename C, typename Ret, typename... Args>
struct get_callable_return_type<Ret (C::*)(Args...) noexcept>
{
    using type = Ret;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_METHOD_CALLABLE_TRAITS_IMPL_H
