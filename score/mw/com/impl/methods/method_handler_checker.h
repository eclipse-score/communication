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
#ifndef SCORE_MW_COM_IMPL_METHODS_METHOD_HANDLER_CHECKER_H
#define SCORE_MW_COM_IMPL_METHODS_METHOD_HANDLER_CHECKER_H

#include <tuple>
#include <type_traits>

namespace score::mw::com::impl
{
namespace detail
{

template <typename T>
struct get_method_in_arg_and_return_types : public get_method_in_arg_and_return_types<decltype(&T::operator())>
{
};

template <typename C, typename Ret, typename RetArg, typename... InArgs>
struct get_method_in_arg_and_return_types<Ret (C::*)(RetArg, InArgs...) const>
{
    using return_type = RetArg;
    using in_args = std::tuple<InArgs...>;
};

template <typename T>
struct get_method_in_arg_types : public get_method_in_arg_types<decltype(&T::operator())>
{
};

template <typename C, typename Ret, typename... InArgs>
struct get_method_in_arg_types<Ret (C::*)(InArgs...) const>
{
    using type = std::tuple<InArgs...>;
};

template <typename T>
struct get_method_return_type : public get_method_return_type<decltype(&T::operator())>
{
};

template <typename C, typename Ret, typename RetArg>
struct get_method_return_type<Ret (C::*)(RetArg) const>
{
    using type = RetArg;
};

template <typename T>
struct get_callable_return_type : public get_callable_return_type<decltype(&T::operator())>
{
};

template <typename C, typename Ret, typename... Args>
struct get_callable_return_type<Ret (C::*)(Args...) const>
{
    using type = Ret;
};

}  // namespace detail

template <typename Callable, typename ReturnType, typename... ArgTypes>
constexpr void AssertCallableMatchesMethodSignature()
{
    using CallableReturnType = typename detail::get_callable_return_type<Callable>::type;
    static_assert(std::is_same_v<CallableReturnType, void>,
                  "Registered method callable must have void return type! The actual method return type is passed as "
                  "first argument to the callable as non-const reference!");

    constexpr bool has_in_args = (sizeof...(ArgTypes) != 0);
    constexpr bool has_return_type = !std::is_same_v<ReturnType, void>;
    if constexpr (has_in_args && has_return_type)
    {
        using MethodInArgTuple = typename detail::get_method_in_arg_and_return_types<Callable>::in_args;
        using MethodReturnType = typename detail::get_method_in_arg_and_return_types<Callable>::return_type;

        using ExpectedInArgTypeTuple = std::tuple<const ArgTypes&...>;
        using ExpectedReturnType = ReturnType&;

        static_assert(
            std::is_same_v<MethodReturnType, ExpectedReturnType>,
            "Registered method callable must have the method return type as first argument as non-const reference!");
        static_assert(std::is_same_v<MethodInArgTuple, ExpectedInArgTypeTuple>,
                      "Registered method callable must have the same in argument types as the method signature "
                      "following the return type, but with const reference semantics!");
    }
    else if constexpr (!has_in_args && has_return_type)
    {
        using MethodCallableReturnType = typename detail::get_method_return_type<Callable>::type;
        using ExpectedReturnType = ReturnType&;

        static_assert(
            std::is_same_v<MethodCallableReturnType, ExpectedReturnType>,
            "Registered method callable must have the method return type as only argument as non-const reference!");
    }
    else if constexpr (has_in_args && !has_return_type)
    {
        using MethodCallableInArgTuple = typename detail::get_method_in_arg_types<Callable>::type;
        using ExpectedInArgTypeTuple = std::tuple<const ArgTypes&...>;

        static_assert(std::is_same_v<MethodCallableInArgTuple, ExpectedInArgTypeTuple>,
                      "Registered method callable must have only the in argument types from the method signature, but "
                      "with const reference semantics!");
    }
    else
    {
        using MethodCallableInArgTuple = typename detail::get_method_in_arg_types<Callable>::type;
        static_assert(std::is_same_v<MethodCallableInArgTuple, std::tuple<>>,
                      "Registered method callable must not have any arguments since the method signature does not "
                      "specify any in arguments or return type!");
    }
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_METHOD_HANDLER_CHECKER_H
