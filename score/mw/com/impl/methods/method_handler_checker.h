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

/**
#include <iostream>
#include <array>
#include <cstdint>
#include <map>
#include <type_traits>
#include <functional>

// template <typename TupleTypes, typename... Ts>
// struct are_same;

// template <typename TupleTypes, typename Head, typename... Tail>
// struct are_same<TupleTypes, Head, Tail...>
// {
//     static const bool value = std::is_same_v<Head> && are_same<Tail...>::value;
// };

// template <typename TupleTypes, typename T>
// struct are_same<TupleTypes, T>
// {
//     static const bool value = std::is_same_v<T>;
// };



template <typename... Ts>
struct are_const_lvalue_references;

template <typename Head, typename... Tail>
struct are_const_lvalue_references<Head, Tail...>
{
    static const bool value = ((std::is_lvalue_reference_v<Head> && std::is_const_v<std::remove_reference_t<Head>>)
        && are_const_lvalue_references<Tail...>::value);
};

template <typename T>
struct are_const_lvalue_references<T>
{
    static const bool value = std::is_lvalue_reference_v<T> && std::is_const_v<std::remove_reference_t<T>>;
};

template <typename T>
struct assert_callable_with_in_args_and_return : public
assert_callable_with_in_args_and_return<decltype(&T::operator())> {};

template <typename C, typename Ret, typename RetArg, typename... Args>
struct assert_callable_with_in_args_and_return<Ret(C::*)(RetArg, Args...) const> {
     static_assert(std::is_same_v<Ret, void>);
     static_assert(std::is_lvalue_reference_v<RetArg> && !std::is_const_v<std::remove_reference_t<RetArg>>);
     static_assert(are_const_lvalue_references<Args...>::value);
};


template <typename T>
struct assert_callable_with_in_args_only : public assert_callable_with_in_args_only<decltype(&T::operator())> {};

template <typename C, typename Ret, typename... Args>
struct assert_callable_with_in_args_only<Ret(C::*)(Args...) const> {
     static_assert(std::is_same_v<Ret, void>);
     static_assert(are_const_lvalue_references<Args...>::value);
};



template <typename T>
struct assert_callable_with_return_only : public assert_callable_with_return_only<decltype(&T::operator())> {};

template <typename C, typename Ret, typename RetArg>
struct assert_callable_with_return_only<Ret(C::*)(RetArg) const> {
     static_assert(std::is_same_v<Ret, void>);
     static_assert(std::is_lvalue_reference_v<RetArg> && !std::is_const_v<std::remove_reference_t<RetArg>>);
    //  static_assert(are_const_lvalue_references<Args...>::value);
};




template <typename T>
struct assert_callable_without_in_args_or_return : public
assert_callable_without_in_args_or_return<decltype(&T::operator())> {};

template <typename C, typename Ret>
struct assert_callable_without_in_args_or_return<Ret(C::*)() const> {
     static_assert(std::is_same_v<Ret, void>);
    //  static_assert(std::is_lvalue_reference_v<RetArg> && !std::is_const_v<std::remove_reference_t<RetArg>>);
    //  static_assert(are_const_lvalue_references<Args...>::value);
};


// template <typename Callable, typename ReturnType, typename... ArgTypes>
// struct generic_assert : public generic_assert<ReturnType, ArgTypes..., decltype(&Callable::operator())> {};

// template <typename ReturnType, typename... ArgTypes, typename C, typename Ret, typename RetArg, typename... Args>
// struct generic_assert<ReturnType, ArgTypes..., Ret(C::*)(RetArg, Args...) const> {
//      static_assert(std::is_same_v<Ret, void>);
//      static_assert(std::is_lvalue_reference_v<RetArg> && !std::is_const_v<std::remove_reference_t<RetArg>>);
//      static_assert(are_const_lvalue_references<Args...>::value);
// };


template <typename T>
struct get_method_in_arg_type_tuple : public get_method_in_arg_type_tuple<decltype(&T::operator())> {};

template <typename C, typename Ret, typename RetArg, typename... Args>
struct get_method_in_arg_type_tuple<Ret(C::*)(RetArg, Args...) const> {
    using type = std::tuple<Args...>;
};


template <typename T>
struct get_method_return_type : public get_method_return_type<decltype(&T::operator())> {};

template <typename C, typename Ret, typename RetArg, typename... Args>
struct get_method_return_type<Ret(C::*)(RetArg, Args...) const> {
    using type = RetArg;
};


template <typename ReturnType, typename... ArgTypes>
class SkeletonMethod{
public:
    template <typename Callable>
    void func(Callable callable)
    {
        constexpr bool has_in_args = (sizeof...(ArgTypes) != 0);
        constexpr bool has_return_type = !std::is_same_v<ReturnType, void>;

        if constexpr(has_in_args)
        {
            // using ExpectedInArgsSignature = const ArgTypes&...;
            // using ExpectedReturnSignature = ReturnType&;

            using CallableReturnType = typename get_method_return_type<Callable>::type;
            using CallableInArgsTuple = typename get_method_in_arg_type_tuple<Callable>::type;

            using ExpectedInArgsTuple = std::tuple<const ArgTypes&...>;
            using ExpectedReturnType = std::tuple<const ArgTypes&...>;

            static_assert(std::is_same_v<CallableReturnType, ReturnType&>);
            static_assert(std::is_same_v<CallableInArgsTuple, ExpectedTuple>);
        }
        else if constexpr(has_in_args && !has_return_type)
        {
            assert_callable_with_in_args_only<Callable>{};
        }
        else if constexpr(!has_in_args && has_return_type)
        {
            assert_callable_with_return_only<Callable>{};
        }
        else
        {
            assert_callable_without_in_args_or_return<Callable>{};
        }

    }
};

int main()
{
    // SkeletonMethod<int, int, double> method{};
    // method.func([](int& ret, const int& value, const double& value2){});

    // using TupleType = get_method_in_arg_type_tuple<std::function<void(int&, float)>>::type;
    // int j{};
    // TupleType val{j, 2.0};

    // std::tuple<int> v{1};

    SkeletonMethod<int, int, double> method{};
    method.func([](int&, const int& value, const double& v2){});
    // method.func([](int&, const int& value){});

    SkeletonMethod<void, int, double> method2{};
    method2.func([](const int& value, const double& v2){});
    // method2.func([](const int& value){});

    SkeletonMethod<int> method3{};
    method3.func([](int&){});
    // // method3.func([](int&, const int&){});

    SkeletonMethod<void> method4{};
    method4.func([](){});
    // method4.func([](int){});

    static_assert(std::is_const_v<std::remove_reference_t<const int&>>);
    static_assert(are_const_lvalue_references<const int&>::value);
    static_assert(are_const_lvalue_references<const int&, const bool&>::value);
    // assertion<std::function<void(const int&, const double&)>>{};
    // assertion<std::function<void(int&, const int&, const double&)>>{};

    // option([](int){});
    // option(std::function<int(int)>{});
    return 0;
}

**/

/**
#include <iostream>
#include <array>
#include <cstdint>
#include <map>
#include <type_traits>
#include <functional>

// template <typename TupleTypes, typename... Ts>
// struct are_same;

// template <typename TupleTypes, typename Head, typename... Tail>
// struct are_same<TupleTypes, Head, Tail...>
// {
//     static const bool value = std::is_same_v<Head> && are_same<Tail...>::value;
// };

// template <typename TupleTypes, typename T>
// struct are_same<TupleTypes, T>
// {
//     static const bool value = std::is_same_v<T>;
// };



template <typename... Ts>
struct are_const_lvalue_references;

template <typename Head, typename... Tail>
struct are_const_lvalue_references<Head, Tail...>
{
    static const bool value = ((std::is_lvalue_reference_v<Head> && std::is_const_v<std::remove_reference_t<Head>>)
        && are_const_lvalue_references<Tail...>::value);
};

template <typename T>
struct are_const_lvalue_references<T>
{
    static const bool value = std::is_lvalue_reference_v<T> && std::is_const_v<std::remove_reference_t<T>>;
};

template <typename T>
struct assert_callable_with_in_args_and_return : public
assert_callable_with_in_args_and_return<decltype(&T::operator())> {};

template <typename C, typename Ret, typename RetArg, typename... Args>
struct assert_callable_with_in_args_and_return<Ret(C::*)(RetArg, Args...) const> {
     static_assert(std::is_same_v<Ret, void>);
     static_assert(std::is_lvalue_reference_v<RetArg> && !std::is_const_v<std::remove_reference_t<RetArg>>);
     static_assert(are_const_lvalue_references<Args...>::value);
};


template <typename T>
struct assert_callable_with_in_args_only : public assert_callable_with_in_args_only<decltype(&T::operator())> {};

template <typename C, typename Ret, typename... Args>
struct assert_callable_with_in_args_only<Ret(C::*)(Args...) const> {
     static_assert(std::is_same_v<Ret, void>);
     static_assert(are_const_lvalue_references<Args...>::value);
};



template <typename T>
struct assert_callable_with_return_only : public assert_callable_with_return_only<decltype(&T::operator())> {};

template <typename C, typename Ret, typename RetArg>
struct assert_callable_with_return_only<Ret(C::*)(RetArg) const> {
     static_assert(std::is_same_v<Ret, void>);
     static_assert(std::is_lvalue_reference_v<RetArg> && !std::is_const_v<std::remove_reference_t<RetArg>>);
    //  static_assert(are_const_lvalue_references<Args...>::value);
};




template <typename T>
struct assert_callable_without_in_args_or_return : public
assert_callable_without_in_args_or_return<decltype(&T::operator())> {};

template <typename C, typename Ret>
struct assert_callable_without_in_args_or_return<Ret(C::*)() const> {
     static_assert(std::is_same_v<Ret, void>);
    //  static_assert(std::is_lvalue_reference_v<RetArg> && !std::is_const_v<std::remove_reference_t<RetArg>>);
    //  static_assert(are_const_lvalue_references<Args...>::value);
};


// template <typename Callable, typename ReturnType, typename... ArgTypes>
// struct generic_assert : public generic_assert<ReturnType, ArgTypes..., decltype(&Callable::operator())> {};

// template <typename ReturnType, typename... ArgTypes, typename C, typename Ret, typename RetArg, typename... Args>
// struct generic_assert<ReturnType, ArgTypes..., Ret(C::*)(RetArg, Args...) const> {
//      static_assert(std::is_same_v<Ret, void>);
//      static_assert(std::is_lvalue_reference_v<RetArg> && !std::is_const_v<std::remove_reference_t<RetArg>>);
//      static_assert(are_const_lvalue_references<Args...>::value);
// };


template <typename T>
struct get_method_in_arg_and_return_types : public get_method_in_arg_and_return_types<decltype(&T::operator())> {};

template <typename C, typename Ret, typename RetArg, typename... InArgs>
struct get_method_in_arg_and_return_types<Ret(C::*)(RetArg, InArgs...) const> {
    using return_type = RetArg;
    using in_args =  std::tuple<InArgs...>;
};


template <typename T>
struct get_method_in_arg_types : public get_method_in_arg_types<decltype(&T::operator())> {};

template <typename C, typename Ret, typename... InArgs>
struct get_method_in_arg_types<Ret(C::*)(InArgs...) const> {
    using type =  std::tuple<InArgs...>;
};


template <typename T>
struct get_method_return_type : public get_method_return_type<decltype(&T::operator())> {};

template <typename C, typename Ret, typename RetArg>
struct get_method_return_type<Ret(C::*)(RetArg) const> {
    using type = RetArg;
};


template <typename T>
struct get_callable_return_type : public get_callable_return_type<decltype(&T::operator())> {};

template <typename C, typename Ret, typename... Args>
struct get_callable_return_type<Ret(C::*)(Args...) const> {
    using type = Ret;
};



// template <typename Return, typename... InArgs>
// struct get_in_args_and_return
// {
//     using return_type = Return;
//     using in_args =  std::tuple<InArgs...>;
// };




// template <typename T>
// struct get_method_in_arg_type_tuple : public get_method_in_arg_type_tuple<decltype(&T::operator())> {};

// template <typename C, typename Ret, typename RetArg, typename... Args>
// struct get_method_in_arg_type_tuple<Ret(C::*)(RetArg, Args...) const> {
//     using type = std::tuple<Args...>;
// };


// template <typename T>
// struct get_method_return_type : public get_method_return_type<decltype(&T::operator())> {};

// template <typename C, typename Ret, typename RetArg, typename... Args>
// struct get_method_return_type<Ret(C::*)(RetArg, Args...) const> {
//     using type = RetArg;
// };


template <typename ReturnType, typename... ArgTypes>
class SkeletonMethod{
public:
    template <typename Callable>
    void func(Callable callable)
    {
        constexpr bool has_in_args = (sizeof...(ArgTypes) != 0);
        constexpr bool has_return_type = !std::is_same_v<ReturnType, void>;

        using CallableReturnType = typename get_callable_return_type<Callable>::type;
        static_assert(std::is_same_v<CallableReturnType, void>);

        if constexpr(has_in_args && has_return_type)
        {
            using MethodInArgTuple = typename get_method_in_arg_and_return_types<Callable>::in_args;
            using MethodReturnType = typename get_method_in_arg_and_return_types<Callable>::return_type;

            using ExpectedInArgTypeTuple = std::tuple<const ArgTypes&...>;
            using ExpectedReturnType = ReturnType&;

            static_assert(std::is_same_v<MethodInArgTuple, ExpectedInArgTypeTuple>);
            static_assert(std::is_same_v<MethodReturnType, ExpectedReturnType>);
        }
        else if constexpr(!has_in_args && has_return_type)
        {
            using MethodCallableReturnType = typename get_method_return_type<Callable>::type;
            using ExpectedReturnType = ReturnType&;

            static_assert(std::is_same_v<MethodCallableReturnType, ExpectedReturnType>);
        }
        else if constexpr(has_in_args && !has_return_type)
        {
            using MethodCallableInArgTuple = typename get_method_in_arg_types<Callable>::type;
            using ExpectedInArgTypeTuple = std::tuple<const ArgTypes&...>;

            static_assert(std::is_same_v<MethodCallableInArgTuple, ExpectedInArgTypeTuple>);
        }
        else
        {
            using MethodCallableInArgTuple = typename get_method_in_arg_types<Callable>::type;
            static_assert(std::is_same_v<MethodCallableInArgTuple, std::tuple<>>);
        }
    }
};

int main()
{
    // SkeletonMethod<int, int, double> method{};
    // method.func([](int& ret, const int& value, const double& value2){});

    // using TupleType = get_method_in_arg_type_tuple<std::function<void(int&, float)>>::type;
    // int j{};
    // TupleType val{j, 2.0};

    // std::tuple<int> v{1};

    SkeletonMethod<int, int, double> method{};
    method.func([](int&, const int&, const double&){});

    SkeletonMethod<void, int, double> method2{};
    method2.func([](const int& value, const double& v2){});
    // method2.func([](const int& value){});

    SkeletonMethod<int> method3{};
    method3.func([](int&){});
    // method3.func([](const int&){});

    SkeletonMethod<void> method4{};
    method4.func([](){});
    // method4.func([](int){});

    static_assert(std::is_const_v<std::remove_reference_t<const int&>>);
    static_assert(are_const_lvalue_references<const int&>::value);
    static_assert(are_const_lvalue_references<const int&, const bool&>::value);
    // assertion<std::function<void(const int&, const double&)>>{};
    // assertion<std::function<void(int&, const int&, const double&)>>{};

    // option([](int){});
    // option(std::function<int(int)>{});
    return 0;
}

**/
