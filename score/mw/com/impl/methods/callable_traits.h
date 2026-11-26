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
#ifndef SCORE_MW_COM_IMPL_METHODS_METHOD_CALLABLE_TRAITS_H
#define SCORE_MW_COM_IMPL_METHODS_METHOD_CALLABLE_TRAITS_H

#include <tuple>
#include <type_traits>

namespace score::mw::com::impl
{

/// \brief Implementation of std::remove_cvref which is only available in C++20.
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

/// \brief Helper struct to check if a tuple type is empty.
template <typename T, typename = void>
struct is_tuple_empty : std::false_type
{
};

template <typename T>
struct is_tuple_empty<T, std::enable_if_t<std::tuple_size_v<T> == 0U>> : std::true_type
{
};

template <typename T>
using is_tuple_empty_t = typename is_tuple_empty<T>::type;

/// \brief Helper struct to extract the tail of a tuple (i.e. all elements except the first one) as a tuple type.
template <typename... T>
struct get_tuple_tail;

template <typename Head, typename... Tail>
struct get_tuple_tail<std::tuple<Head, Tail...>>
{
    using type = std::tuple<Tail...>;
};

template <typename T>
using get_tuple_tail_t = typename get_tuple_tail<T>::type;

/// \brief Helper struct to extract the in argument types of a callable as a tuple type in member alias `type`.
///
/// e.g. for a callable of type `int(const double&, int, const std::string), `get_callable_args_types<int(const
/// double&, int, const std::string)>::type` will be `std::tuple<const double&, int, const std::string>`.
template <typename T>
struct get_callable_args_types : public get_callable_args_types<decltype(&remove_cvref_t<T>::operator())>
{
};

/// \brief Helper struct to extract the return type of a callable as a type in member alias `type`.
///
/// e.g. for a callable of type `int(const double&, int, const std::string), `get_callable_return_type<int(const
/// double&, int, const std::string)>::type` will be `int`.
template <typename T>
struct get_callable_return_type : public get_callable_return_type<decltype(&remove_cvref_t<T>::operator())>
{
};

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

#endif  // SCORE_MW_COM_IMPL_METHODS_METHOD_CALLABLE_TRAITS_H
