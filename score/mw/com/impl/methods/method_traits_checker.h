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
#ifndef SCORE_MW_COM_IMPL_METHODS_METHOD_TRAITS_CHECKER_H
#define SCORE_MW_COM_IMPL_METHODS_METHOD_TRAITS_CHECKER_H

#include "score/mw/com/impl/methods/callable_traits.h"

#include <score/assert.hpp>

#include <functional>
#include <tuple>
#include <type_traits>

namespace score::mw::com::impl
{
namespace detail
{

/// \brief Utility to extract the method return type from a tuple of the form `std::tuple<ReturnType, InArgTypes...>`
/// The tuple can be extracter from a callabe using `get_callable_args_types`.
///
/// \return type of the first element of the tuple (i.e. `ReturnType`)
template <typename... TupleArgs>
struct get_method_return_type;

template <typename... TupleArgs>
struct get_method_return_type<std::tuple<TupleArgs...>>
{
    using type = std::tuple_element_t<0, std::tuple<TupleArgs...>>;
};

template <typename T>
using get_method_return_type_t = typename get_method_return_type<T>::type;

/// \brief Utility to extract the method in argument types from a tuple of the form `std::tuple<ReturnType,
/// InArgTypes...>`
/// The tuple can be extracter from a callabe using `get_callable_args_types`.
///
/// \return tuple type `std::tuple<InArgTypes...>`
template <typename... TupleArgs>
struct get_method_in_args;

template <typename... TupleArgs>
struct get_method_in_args<std::tuple<TupleArgs...>>
{
    using type = get_tuple_tail_t<std::tuple<TupleArgs...>>;
};

template <typename T>
using get_method_in_args_t = typename get_method_in_args<T>::type;

}  // namespace detail

enum class FailureMode
{
    COMPILE_TIME,
    RUNTIME
};

// TODO: We currently use FailureMode as a workaround to allow testing AssertMethodHandlerSupportsMethodSignature at
// runtime since we don't currently have infrastructure for compile time testing. When compile time testing is enabled
// in SWP-46885, we can remove FailureMode and the runtime tests and replace them with compile time tests.
template <bool condition, FailureMode failure_mode>
void CompileOrRuntimeAssert(const char* message)
{
    if constexpr (failure_mode == FailureMode::COMPILE_TIME)
    {
        static_assert(condition);
    }
    else
    {
        if (!condition)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(condition, message);
        }
    }
}

/// \brief Checks at compile time whether a given callable type matches the expected signature for a method handler.
///
/// For a method signature of the form `ReturnType(ArgTypes...)`, the expected callable signature is:
/// - If `ReturnType` is not `void` and there are in arguments:
///   `void(ReturnType&, const ArgTypes&...)`
/// - If `ReturnType` is not `void` and there are no in arguments:
///   `void(ReturnType&)`
/// - If `ReturnType` is `void` and there are in arguments:
///   `void(const ArgTypes&...)`
/// - If `ReturnType` is `void` and there are no in arguments:
///   `void()`
template <FailureMode FailureMode, typename Callable, typename ReturnType, typename... ArgTypes>
constexpr void AssertMethodHandlerSupportsMethodSignature()
{
    using CallableReturnType = typename get_callable_return_type<Callable>::type;
    CompileOrRuntimeAssert<std::is_same_v<CallableReturnType, void>, FailureMode>(
        "Registered method callable must have void return type! The actual method return type is passed as "
        "first argument to the callable as non-const reference!");

    using CallableArgs = typename get_callable_args_types<Callable>::type;

    constexpr bool has_in_args = (sizeof...(ArgTypes) != 0);
    constexpr bool has_return_type = !std::is_same_v<ReturnType, void>;

    // Since we want to be able to check assertions at runtime during testing using CompileOrRuntimeAssert, we have to
    // avoid compile time assertion failures in other places. Anything in the else branch of a constexpr if statement
    // will not be compiled if the condition is false. So we use else branches instead of early returns. Once compile
    // time testing is enabled in SWP-46885, we can likely simplify the branching in this code.
    if constexpr (!has_in_args && !has_return_type)
    {
        CompileOrRuntimeAssert<std::is_same_v<CallableArgs, std::tuple<>>, FailureMode>(
            "Registered method callable must not have any arguments since the method signature does not specify "
            "any in arguments or return type!");
    }
    else
    {
        if constexpr (is_tuple_empty<CallableArgs>::value)
        {
            CompileOrRuntimeAssert<!is_tuple_empty<CallableArgs>::value, FailureMode>(
                "Registered method callable must have at least one argument (a return type and/or one or more in "
                "args)");
        }
        else
        {

            if constexpr (has_in_args && has_return_type)
            {
                using MethodInArgTuple = detail::get_method_in_args_t<CallableArgs>;
                using MethodReturnType = detail::get_method_return_type_t<CallableArgs>;

                using ExpectedInArgTypeTuple = std::tuple<const ArgTypes&...>;
                using ExpectedReturnType = ReturnType&;

                CompileOrRuntimeAssert<std::is_same_v<MethodReturnType, ExpectedReturnType>, FailureMode>(
                    "Registered method callable must have the method return type as first argument as non-const "
                    "reference!");
                CompileOrRuntimeAssert<std::is_same_v<MethodInArgTuple, ExpectedInArgTypeTuple>, FailureMode>(
                    "Registered method callable must have the same in argument types as the method signature "
                    "following the return type, but with const reference semantics!");
            }
            else if constexpr (!has_in_args && has_return_type)
            {
                using MethodReturnType = detail::get_method_return_type_t<CallableArgs>;
                using ExpectedReturnType = ReturnType&;

                CompileOrRuntimeAssert<std::is_same_v<MethodReturnType, ExpectedReturnType>, FailureMode>(
                    "Registered method callable must have the method return type as only argument as non-const "
                    "reference "
                    "!");
            }
            else if constexpr (has_in_args && !has_return_type)
            {
                using MethodInArgTuple = CallableArgs;
                using ExpectedInArgTypeTuple = std::tuple<const ArgTypes&...>;

                CompileOrRuntimeAssert<std::is_same_v<MethodInArgTuple, ExpectedInArgTypeTuple>, FailureMode>(
                    "Registered method callable must have only the in argument types from the method signature, but "
                    "with const reference semantics!");
            }
        }
    }
}

/// \brief Checks at compile time that the given callable is not a std::bind expression.
///
/// std::bind expressions make checking the callable signature in AssertMethodHandlerSupportsMethodSignature difficult.
/// Since the functionality of std::bind can be achieved using lambdas (which is even recommended over std::bind in
/// general), we disallow std::bind expressions as method handlers.
template <FailureMode FailureMode, typename Callable>
constexpr void AssertMethodCallableIsNotStdBind()
{
    CompileOrRuntimeAssert<!std::is_bind_expression_v<std::remove_reference_t<Callable>>, FailureMode>(
        "std::bind expressions are not allowed as method handlers as they are not supported by "
        "AssertMethodHandlerSupportsMethodSignature which checks the callable signature. Please use a lambda instead.");
}

/// \brief Checks at compile time that the method signature does not contain pointers or references in the return type
/// or in args types.
///
/// Pointers and references in a handler will only be valid in the process which initializes them. Since method return
/// types and in args are placed in shared memory, we disallow pointers and references.
template <FailureMode FailureMode, typename ReturnType, typename... ArgTypes>
constexpr void AssertMethodSignatureDoesNotContainPointersOrReferences()
{
    CompileOrRuntimeAssert<!std::is_pointer_v<ReturnType>, FailureMode>("Method return type must not be a pointer!");
    CompileOrRuntimeAssert<!std::is_reference_v<ReturnType>, FailureMode>(
        "Method return type must not be a reference!");

    constexpr bool in_args_are_not_pointers = (... && !std::is_pointer_v<ArgTypes>);
    CompileOrRuntimeAssert<in_args_are_not_pointers, FailureMode>("Method in args must not be pointers!");

    constexpr bool in_args_are_not_references = (... && !std::is_reference_v<ArgTypes>);
    CompileOrRuntimeAssert<in_args_are_not_references, FailureMode>("Method in args must not be references!");
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_METHOD_TRAITS_CHECKER_H
