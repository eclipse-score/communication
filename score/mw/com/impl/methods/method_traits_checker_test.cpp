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
#include "score/mw/com/impl/methods/method_traits_checker.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <functional>

namespace score::mw::com::impl
{
namespace
{

TEST(MethodTraitsCheckerReturnAndInArgsCompileTimeTest, CallingAssertCallableWithCallableDoesNotTerminate)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts the return and InArgs.
    using Callable = std::function<void(MethodReturnType&, const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<FailureMode::COMPILE_TIME,
                                         Callable,
                                         MethodReturnType,
                                         FirstInArgType,
                                         SecondInArgType>();
}

TEST(MethodTraitsCheckerReturnOnlyCompileTimeTest, CallingAssertCallableWithCallableDoesNotTerminate)
{
    // Given a method with a return type and no InArgs.
    using MethodReturnType = int;

    // and given a callable that accepts the return and no InArgs.
    using Callable = std::function<void(MethodReturnType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<FailureMode::COMPILE_TIME, Callable, MethodReturnType>();
}

TEST(MethodTraitsCheckerInArgsOnlyCompileTimeTest, CallingAssertCallableWithCallableDoesNotTerminate)
{
    // Given a method with no return type and InArgs.
    using MethodReturnType = void;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts no return and InArgs.
    using Callable = std::function<void(const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<FailureMode::COMPILE_TIME,
                                         Callable,
                                         MethodReturnType,
                                         FirstInArgType,
                                         SecondInArgType>();
}

TEST(MethodTraitsCheckerNoReturnAndNoInArgsCompileTimeTest, CallingAssertCallableWithCallableDoesNotTerminate)
{
    // Given a method with no return type and no InArgs.
    using MethodReturnType = void;

    // and given a callable that accepts no return and no InArgs.
    using Callable = std::function<void()>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<FailureMode::COMPILE_TIME, Callable, MethodReturnType>();
}

TEST(MethodTraitsCheckerGetMethodReturnType, CallingWithTupleContainingOneElementReturnsFirstElement)
{
    using ExpectedMethodReturnType = int;
    using MethodArgsTuple = std::tuple<ExpectedMethodReturnType>;

    // When extracting the method return type from a tuple containing a single element
    using ActualMethodReturnType = detail::get_method_return_type_t<MethodArgsTuple>;

    // Then the extracted method return type is the same as the first element of the tuple
    EXPECT_TRUE((std::is_same_v<ActualMethodReturnType, ExpectedMethodReturnType>));
}

TEST(MethodTraitsCheckerGetMethodReturnType, CallingWithTupleContainingMultipleElementsReturnsFirstElement)
{
    using ExpectedMethodReturnType = int;
    using MethodArgsTuple = std::tuple<ExpectedMethodReturnType, double, std::string>;

    // When extracting the method return type from a tuple containing multiple elements
    using ActualMethodReturnType = detail::get_method_return_type_t<MethodArgsTuple>;

    // Then the extracted method return type is the same as the first element of the tuple
    EXPECT_TRUE((std::is_same_v<ActualMethodReturnType, ExpectedMethodReturnType>));
}

// TODO: Tests for invalid callables that should cause static assertion failures. These tests are disabled since we
// currently don't have infrastructure for compile time testing. To be enabled in SWP-46885.
#if 0

TEST(MethodTraitsCheckerGetMethodReturnType, CallingWithEmptyTupleFailsToCompile)
{
    // When extracting the method return type from an empty tuple
    // Then we should get a compiler error
    using ActualMethodReturnType = detail::get_method_return_type_t<std::tuple<>>;
}

TEST(MethodTraitsCheckerGetMethodReturnType, CallingWithCallableFailsToCompile)
{
    // When extracting the method return type from a callable type
    // Then we should get a compiler error
    using ActualMethodReturnType = detail::get_method_return_type_t<std::function<void()>>;
}

#endif

TEST(MethodTraitsCheckerGetMethodInArgType, CallingWithTupleContainingOneElementReturnsEmptyTuple)
{
    using MethodArgsTuple = std::tuple<int>;

    // When extracting the method in argument types from a tuple containing a single element
    using ActualMethodInArgTypes = detail::get_method_in_args_t<MethodArgsTuple>;

    // Then the extracted method in argument types is an empty tuple (since the first argument of the tuple is the
    // return type)
    EXPECT_TRUE((std::is_same_v<ActualMethodInArgTypes, std::tuple<>>));
}

TEST(MethodTraitsCheckerGetMethodInArgType, CallingWithTupleContainingMultipleElementsReturnsTupleTail)
{
    using ExpectedMethodInArgTypes = std::tuple<double, std::string>;
    using MethodArgsTuple = std::tuple<int, double, std::string>;

    // When extracting the method in argument types from a tuple containing multiple elements
    using ActualMethodInArgTypes = detail::get_method_in_args_t<MethodArgsTuple>;

    // Then the extracted method in argument types is a tuple containing all elements of the original tuple except the
    // first element (since the first element of the tuple is the return type)
    EXPECT_TRUE((std::is_same_v<ActualMethodInArgTypes, ExpectedMethodInArgTypes>));
}

// TODO: Tests for invalid callables that should cause static assertion failures. These tests are disabled since we
// currently don't have infrastructure for compile time testing. To be enabled in SWP-46885.
#if 0

TEST(MethodTraitsCheckerGetMethodInArgType, CallingWithEmptyTupleFailsToCompile)
{
    // When extracting the method in argument types from an empty tuple
    // Then we should get a compiler error
    using ActualMethodInArgTypes = detail::get_method_in_args_t<std::tuple<>>;
}

TEST(MethodTraitsCheckerGetMethodInArgType, CallingWithCallableFailsToCompile)
{
    // When extracting the method in argument types from a callable type
    // Then we should get a compiler error
    using ActualMethodInArgTypes = detail::get_method_in_args_t<std::function<void()>>;
}

#endif

// TODO: We currently use FailureMode as a workaround to allow testing AssertCallableMatchesMethodSignature at runtime
// since we don't currently have infrastructure for compile time testing. When compile time testing is enabled in
// SWP-46885, we can remove FailureMode and the runtime tests and replace them with compile time tests.
TEST(MethodTraitsCheckerReturnAndInArgsRuntimeTest, CallingAssertCallableWithCallableDoesNotTerminate)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts the return and InArgs.
    using Callable = std::function<void(MethodReturnType&, const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<FailureMode::RUNTIME,
                                         Callable,
                                         MethodReturnType,
                                         FirstInArgType,
                                         SecondInArgType>();
}

TEST(MethodTraitsCheckerReturnAndInArgsRuntimeTest, CallingAssertCallableWithCallableWithMissingReturnTerminates)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts only the InArgs.
    using Callable = std::function<void(const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED((AssertCallableMatchesMethodSignature<FailureMode::RUNTIME,
                                                                                            Callable,
                                                                                            MethodReturnType,
                                                                                            FirstInArgType,
                                                                                            SecondInArgType>()));
}

TEST(MethodTraitsCheckerReturnAndInArgsRuntimeTest, CallingAssertCallableWithCallableWithMissingInArgTerminates)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts only the return and only one of the InArgs.
    using Callable = std::function<void(MethodReturnType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED((AssertCallableMatchesMethodSignature<FailureMode::RUNTIME,
                                                                                            Callable,
                                                                                            MethodReturnType,
                                                                                            FirstInArgType,
                                                                                            SecondInArgType>()));
}

TEST(MethodTraitsCheckerReturnAndInArgsRuntimeTest, CallingAssertCallableWithCallableWithConstReturnTypeRefTerminates)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts the correct arguments but the return type is a const reference instead of
    // non-const reference.
    using Callable = std::function<void(const MethodReturnType&, const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED((AssertCallableMatchesMethodSignature<FailureMode::RUNTIME,
                                                                                            Callable,
                                                                                            MethodReturnType,
                                                                                            FirstInArgType,
                                                                                            SecondInArgType>()));
}

TEST(MethodTraitsCheckerReturnAndInArgsRuntimeTest, CallingAssertCallableWithCallableWithNonConstInArgRefTerminates)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts the correct arguments but one of the InArgs is a non-const reference instead of
    // a const reference.
    using Callable = std::function<void(const MethodReturnType&, FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED((AssertCallableMatchesMethodSignature<FailureMode::RUNTIME,
                                                                                            Callable,
                                                                                            MethodReturnType,
                                                                                            FirstInArgType,
                                                                                            SecondInArgType>()));
}

TEST(MethodTraitsCheckerReturnOnlyRuntimeTest, CallingAssertCallableWithCallableDoesNotTerminate)
{
    // Given a method with a return type and no InArgs.
    using MethodReturnType = int;

    // and given a callable that accepts the return and no InArgs.
    using Callable = std::function<void(MethodReturnType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<FailureMode::RUNTIME, Callable, MethodReturnType>();
}

TEST(MethodTraitsCheckerReturnOnlyRuntimeTest, CallingAssertCallableWithCallableWithMissingReturnTerminates)
{
    // Given a method with a return type and no InArgs.
    using MethodReturnType = int;

    // and given a callable that accepts no return and no InArgs.
    using Callable = std::function<void()>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        (AssertCallableMatchesMethodSignature<FailureMode::RUNTIME, Callable, MethodReturnType>()));
}

TEST(MethodTraitsCheckerReturnOnlyRuntimeTest, CallingAssertCallableWithCallableWithConstReturnTypeRefTerminates)
{
    // Given a method with a return type and no InArgs.
    using MethodReturnType = int;

    // and given a callable that accepts the return type as const reference instead of non-const reference.
    using Callable = std::function<void(const MethodReturnType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        (AssertCallableMatchesMethodSignature<FailureMode::RUNTIME, Callable, MethodReturnType>()));
}

TEST(MethodTraitsCheckerInArgsOnlyRuntimeTest, CallingAssertCallableWithCallableDoesNotTerminate)
{
    // Given a method with no return type and InArgs.
    using MethodReturnType = void;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts no return and InArgs.
    using Callable = std::function<void(const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<FailureMode::RUNTIME,
                                         Callable,
                                         MethodReturnType,
                                         FirstInArgType,
                                         SecondInArgType>();
}

TEST(MethodTraitsCheckerInArgsOnlyRuntimeTest, CallingAssertCallableWithCallableWithMissingInArgTerminates)
{
    // Given a method with no return type and InArgs.
    using MethodReturnType = void;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts only one InArg.
    using Callable = std::function<void(const FirstInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED((AssertCallableMatchesMethodSignature<FailureMode::RUNTIME,
                                                                                            Callable,
                                                                                            MethodReturnType,
                                                                                            FirstInArgType,
                                                                                            SecondInArgType>()));
}

TEST(MethodTraitsCheckerInArgsOnlyRuntimeTest, CallingAssertCallableWithCallableWithNonConstInArgRefTerminates)
{
    // Given a method with no return type and InArgs.
    using MethodReturnType = void;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts one of the InArgs as non-const reference instead of const reference.
    using Callable = std::function<void(const FirstInArgType&, SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED((AssertCallableMatchesMethodSignature<FailureMode::RUNTIME,
                                                                                            Callable,
                                                                                            MethodReturnType,
                                                                                            FirstInArgType,
                                                                                            SecondInArgType>()));
}

TEST(MethodTraitsCheckerNoReturnAndNoInArgsRuntimeTest, CallingAssertCallableWithCallableDoesNotTerminate)
{
    // Given a method with no return type and no InArgs.
    using MethodReturnType = void;

    // and given a callable that accepts no return and no InArgs.
    using Callable = std::function<void()>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<FailureMode::RUNTIME, Callable, MethodReturnType>();
}

TEST(MethodTraitsCheckerNoReturnAndNoInArgsRuntimeTest, CallingAssertCallableWithCallableWithInArgTerminates)
{
    // Given a method with no return type and no InArgs.
    using MethodReturnType = void;

    // and given a callable that accepts an InArg.
    using Callable = std::function<void(const int&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        (AssertCallableMatchesMethodSignature<FailureMode::RUNTIME, Callable, MethodReturnType>()));
}

TEST(MethodTraitsCheckerNoReturnAndNoInArgsRuntimeTest, CallingAssertCallableWithCallableWithReturnTypeTerminates)
{
    // Given a method with no return type and no InArgs.
    using MethodReturnType = void;

    // and given a callable that returns a non-void value.
    using Callable = std::function<int()>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        (AssertCallableMatchesMethodSignature<FailureMode::RUNTIME, Callable, MethodReturnType>()));
}

TEST(MethodTraitsCheckerAssertMethodCallableIsNotStdBindCompileTimeTest, CallingWithNonStdBindCallableDoesNotTerminate)
{
    // When calling AssertMethodCallableIsNotStdBind with a non-std::bind type
    // Then the call does not terminate
    auto non_std_bind_callable = []() {};
    AssertMethodCallableIsNotStdBind<FailureMode::COMPILE_TIME, decltype(non_std_bind_callable)>();
}

TEST(MethodTraitsCheckerAssertMethodCallableIsNotStdBindRuntimeTimeTest, CallingWithNonStdBindCallableDoesNotTerminate)
{
    // When calling AssertMethodCallableIsNotStdBind with a non-std::bind type
    // Then the call does not terminate
    auto non_std_bind_callable = []() {};
    AssertMethodCallableIsNotStdBind<FailureMode::RUNTIME, decltype(non_std_bind_callable)>();
}

TEST(MethodTraitsCheckerAssertMethodCallableIsNotStdBindRuntimeTimeTest, CallingWithStdBindTypeTerminates)
{
    // When calling AssertMethodCallableIsNotStdBind with a std::bind type
    // Then the function terminates with a contract violation
    auto std_bind_expression = std::bind([]() {});
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        (AssertMethodCallableIsNotStdBind<FailureMode::RUNTIME, decltype(std_bind_expression)>()));
}

TEST(AssertMethodSignatureDoesNotContainPointersOrReferencesCompileTimeTest,
     CallingWithSignatureWithNoPointerOrReferenceArgsDoesNotTerminate)
{
    // Given a method with no pointer or reference arguments.
    using MethodReturnType = int;
    using FirstInArgType = double;
    using SecondInArgType = std::string;

    // When calling AssertMethodSignatureDoesNotContainPointersOrReferences with a method signature where no argument is
    // a pointer or reference
    // Then the call does not terminate
    AssertMethodSignatureDoesNotContainPointersOrReferences<FailureMode::COMPILE_TIME,
                                                            MethodReturnType,
                                                            FirstInArgType,
                                                            SecondInArgType>();
}

TEST(AssertMethodSignatureDoesNotContainPointersOrReferencesRuntimeTimeTest,
     CallingWithSignatureWithNoPointerOrReferenceArgsDoesNotTerminate)
{
    // Given a method with no pointer or reference arguments.
    using MethodReturnType = int;
    using FirstInArgType = double;
    using SecondInArgType = std::string;

    // When calling AssertMethodSignatureDoesNotContainPointersOrReferences with a method signature where no argument is
    // a pointer or reference
    // Then the call does not terminate
    AssertMethodSignatureDoesNotContainPointersOrReferences<FailureMode::RUNTIME,
                                                            MethodReturnType,
                                                            FirstInArgType,
                                                            SecondInArgType>();
}

TEST(AssertMethodSignatureDoesNotContainPointersOrReferencesRuntimeTimeTest,
     CallingWithSignatureWithPointerArgTerminates)
{
    // Given a method with a pointer argument.
    using MethodReturnType = int;
    using FirstInArgType = double*;
    using SecondInArgType = std::string;

    // When calling AssertMethodSignatureDoesNotContainPointersOrReferences with a method signature where one of the
    // arguments is a pointer
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        (AssertMethodSignatureDoesNotContainPointersOrReferences<FailureMode::RUNTIME,
                                                                 MethodReturnType,
                                                                 FirstInArgType,
                                                                 SecondInArgType>()));
}

TEST(AssertMethodSignatureDoesNotContainPointersOrReferencesRuntimeTimeTest,
     CallingWithSignatureWithPointerReturnTerminates)
{
    // Given a method with a pointer return type.
    using MethodReturnType = double*;
    using FirstInArgType = int;
    using SecondInArgType = std::string;

    // When calling AssertMethodSignatureDoesNotContainPointersOrReferences with a method signature where the return
    // type is a pointer
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        (AssertMethodSignatureDoesNotContainPointersOrReferences<FailureMode::RUNTIME,
                                                                 MethodReturnType,
                                                                 FirstInArgType,
                                                                 SecondInArgType>()));
}

TEST(AssertMethodSignatureDoesNotContainPointersOrReferencesRuntimeTimeTest,
     CallingWithSignatureWithReferenceArgTerminates)
{
    // Given a method with a reference argument.
    using MethodReturnType = int;
    using FirstInArgType = double&;
    using SecondInArgType = std::string;

    // When calling AssertMethodSignatureDoesNotContainPointersOrReferences with a method signature where one of the
    // arguments is a reference
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        (AssertMethodSignatureDoesNotContainPointersOrReferences<FailureMode::RUNTIME,
                                                                 MethodReturnType,
                                                                 FirstInArgType,
                                                                 SecondInArgType>()));
}

TEST(AssertMethodSignatureDoesNotContainPointersOrReferencesRuntimeTimeTest,
     CallingWithSignatureWithReferenceReturnTerminates)
{
    // Given a method with a reference return type.
    using MethodReturnType = double&;
    using FirstInArgType = int;
    using SecondInArgType = std::string;

    // When calling AssertMethodSignatureDoesNotContainPointersOrReferences with a method signature where the return
    // type is a reference
    // Then the function terminates with a contract violation
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        (AssertMethodSignatureDoesNotContainPointersOrReferences<FailureMode::RUNTIME,
                                                                 MethodReturnType,
                                                                 FirstInArgType,
                                                                 SecondInArgType>()));
}

}  // namespace
}  // namespace score::mw::com::impl
