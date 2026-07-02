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
#ifndef SCORE_MW_COM_IMPL_METHODS_TEST_CALLABLE_TRAITS_RESOURCES_H
#define SCORE_MW_COM_IMPL_METHODS_TEST_CALLABLE_TRAITS_RESOURCES_H

namespace score::mw::com::impl
{

/// \brief Factory that default-constructs a CallableWrapper<HandlerSignature> via a static Create().
///
/// \tparam CallableWrapper  A template-template parameter such as score::cpp::callback or std::function.
/// \tparam HandlerSignature The concrete function signature passed to CallableWrapper, e.g. void(int&).
template <template <typename> class, typename>
class CallableFactory;

template <template <typename> class CallableWrapper, typename ReturnType, typename... InArgs>
class CallableFactory<CallableWrapper, ReturnType(InArgs...)>
{
  public:
    static auto Create()
    {
        return CallableWrapper<ReturnType(InArgs...)>{};
    }
};

/// \brief Callable object with a const call operator (no noexcept).
template <typename>
class DummyMethodFunctorConst;

template <typename ReturnType, typename... InArgs>
class DummyMethodFunctorConst<ReturnType(InArgs...)>
{
  public:
    ReturnType operator()(InArgs...) const {}
};

template <typename... InArgs>
class DummyMethodFunctorConst<void(InArgs...)>
{
  public:
    void operator()(InArgs...) const {}
};

/// \brief Callable object with a const noexcept call operator.
template <typename>
class DummyMethodFunctorConstNoexcept;

template <typename ReturnType, typename... InArgs>
class DummyMethodFunctorConstNoexcept<ReturnType(InArgs...)>
{
  public:
    ReturnType operator()(InArgs...) const noexcept {}
};

template <typename... InArgs>
class DummyMethodFunctorConstNoexcept<void(InArgs...)>
{
  public:
    void operator()(InArgs...) const noexcept {}
};

/// \brief Callable object with a non-const call operator (no noexcept).
template <typename>
class DummyMethodFunctorNonConst;

template <typename ReturnType, typename... InArgs>
class DummyMethodFunctorNonConst<ReturnType(InArgs...)>
{
  public:
    ReturnType operator()(InArgs...) {}
};

template <typename... InArgs>
class DummyMethodFunctorNonConst<void(InArgs...)>
{
  public:
    void operator()(InArgs...) {}
};

/// \brief Callable object with a non-const noexcept call operator.
template <typename>
class DummyMethodFunctorNonConstNoexcept;

template <typename ReturnType, typename... InArgs>
class DummyMethodFunctorNonConstNoexcept<ReturnType(InArgs...)>
{
  public:
    ReturnType operator()(InArgs...) noexcept {}
};

template <typename... InArgs>
class DummyMethodFunctorNonConstNoexcept<void(InArgs...)>
{
  public:
    void operator()(InArgs...) noexcept {}
};

/// \brief Factory that creates a plain (non-noexcept, non-mutable) lambda.
template <typename T>
class NonNoexceptMethodLambdaFactory;

template <typename ReturnType, typename... InArgs>
class NonNoexceptMethodLambdaFactory<ReturnType(InArgs...)>
{
  public:
    static auto Create()
    {
        return [](InArgs...) -> ReturnType {
            return ReturnType{};
        };
    }
};

template <typename... InArgs>
class NonNoexceptMethodLambdaFactory<void(InArgs...)>
{
  public:
    static auto Create()
    {
        return [](InArgs...) -> void {};
    }
};

/// \brief Factory that creates a noexcept lambda.
template <typename T>
class NoexceptMethodLambdaFactory;

template <typename ReturnType, typename... InArgs>
class NoexceptMethodLambdaFactory<ReturnType(InArgs...)>
{
  public:
    static auto Create()
    {
        return [](InArgs...) noexcept -> ReturnType {
            return ReturnType{};
        };
    }
};

template <typename... InArgs>
class NoexceptMethodLambdaFactory<void(InArgs...)>
{
  public:
    static auto Create()
    {
        return [](InArgs...) noexcept -> void {};
    }
};

/// \brief Factory that creates a mutable lambda.
template <typename T>
class MutableMethodLambdaFactory;

template <typename ReturnType, typename... InArgs>
class MutableMethodLambdaFactory<ReturnType(InArgs...)>
{
  public:
    static auto Create()
    {
        return [](InArgs...) mutable -> ReturnType {
            return ReturnType{};
        };
    }
};

template <typename... InArgs>
class MutableMethodLambdaFactory<void(InArgs...)>
{
  public:
    static auto Create()
    {
        return [](InArgs...) mutable -> void {};
    }
};

/// \brief Factory that creates a mutable noexcept lambda.
template <typename T>
class MutableNoexceptMethodLambdaFactory;

template <typename ReturnType, typename... InArgs>
class MutableNoexceptMethodLambdaFactory<ReturnType(InArgs...)>
{
  public:
    static auto Create()
    {
        return [](InArgs...) mutable noexcept -> ReturnType {
            return ReturnType{};
        };
    }
};

template <typename... InArgs>
class MutableNoexceptMethodLambdaFactory<void(InArgs...)>
{
  public:
    static auto Create()
    {
        return [](InArgs...) mutable noexcept -> void {};
    }
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_TEST_CALLABLE_TRAITS_RESOURCES_H
