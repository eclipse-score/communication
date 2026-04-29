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
#include "score/mw/com/impl/methods/method_handler_checker.h"

#include <gtest/gtest.h>

#include <functional>

namespace score::mw::com::impl
{
namespace
{

TEST(MethodHandlerCheckerReturnAndInArgsTest, CallingAssertCallableWithValidCallableDoesNotTerminate)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts the return and InArgs.
    using ValidCallable = std::function<void(MethodReturnType&, const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType, FirstInArgType, SecondInArgType>();
}

// TODO: Tests for invalid callables that should cause static assertion failures. These tests are disabled since we
// currently don't have infrastructure for compile time testing. To be enabled in SWP-46885.
#if 0
TEST(MethodHandlerCheckerReturnAndInArgsTest, CallingAssertCallableWithCallableWithMissingReturnTerminates)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts only the InArgs.
    using ValidCallable = std::function<void(const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType, FirstInArgType, SecondInArgType>();
}

TEST(MethodHandlerCheckerReturnAndInArgsTest, CallingAssertCallableWithCallableWithMissingInArgTerminates)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts only the return and only one of the InArgs.
    using ValidCallable = std::function<void(MethodReturnType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType, FirstInArgType, SecondInArgType>();
}

TEST(MethodHandlerCheckerReturnAndInArgsTest, CallingAssertCallableWithCallableWithConstReturnTypeRefTerminates)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts the correct arguments but the return type is a const reference instead of
    // non-const reference.
    using ValidCallable = std::function<void(const MethodReturnType&, const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType, FirstInArgType, SecondInArgType>();
}

TEST(MethodHandlerCheckerReturnAndInArgsTest, CallingAssertCallableWithCallableWithNonConstInArgRefTerminates)
{
    // Given a method with a return type and InArgs.
    using MethodReturnType = int;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts the correct arguments but one of the InArgs is a non-const reference instead of
    // a const reference.
    using ValidCallable = std::function<void(const MethodReturnType&, FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType, FirstInArgType, SecondInArgType>();
}
#endif

TEST(MethodHandlerCheckerReturnOnlyTest, CallingAssertCallableWithValidCallableDoesNotTerminate)
{
    // Given a method with a return type and no InArgs.
    using MethodReturnType = int;

    // and given a callable that accepts the return and no InArgs.
    using ValidCallable = std::function<void(MethodReturnType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType>();
}

// TODO: Tests for invalid callables that should cause static assertion failures. These tests are disabled since we
// currently don't have infrastructure for compile time testing. To be enabled in SWP-46885.
#if 0
TEST(MethodHandlerCheckerReturnOnlyTest, CallingAssertCallableWithCallableWithMissingReturnTerminates)
{
    // Given a method with a return type and no InArgs.
    using MethodReturnType = int;

    // and given a callable that accepts no return and no InArgs.
    using ValidCallable = std::function<void()>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType>();
}

TEST(MethodHandlerCheckerReturnOnlyTest, CallingAssertCallableWithCallableWithConstReturnTypeRefTerminates)
{
    // Given a method with a return type and no InArgs.
    using MethodReturnType = int;

    // and given a callable that accepts the return type as const reference instead of non-const reference.
    using ValidCallable = std::function<void(const MethodReturnType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType>();
}
#endif

TEST(MethodHandlerCheckerInArgsOnlyTest, CallingAssertCallableWithValidCallableDoesNotTerminate)
{
    // Given a method with no return type and InArgs.
    using MethodReturnType = void;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts no return and InArgs.
    using ValidCallable = std::function<void(const FirstInArgType&, const SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType, FirstInArgType, SecondInArgType>();
}

// TODO: Tests for invalid callables that should cause static assertion failures. These tests are disabled since we
// currently don't have infrastructure for compile time testing. To be enabled in SWP-46885.
#if 0
TEST(MethodHandlerCheckerInArgsOnlyTest, CallingAssertCallableWithCallableWithMissingInArgTerminates)
{
    // Given a method with no return type and InArgs.
    using MethodReturnType = void;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts only one InArg.
    using ValidCallable = std::function<void(const FirstInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType, FirstInArgType, SecondInArgType>();
}

TEST(MethodHandlerCheckerInArgsOnlyTest, CallingAssertCallableWithCallableWithNonConstInArgRefTerminates)
{
    // Given a method with no return type and InArgs.
    using MethodReturnType = void;
    using FirstInArgType = int;
    using SecondInArgType = double;

    // and given a callable that accepts one of the InArgs as non-const reference instead of const reference.
    using ValidCallable = std::function<void(const FirstInArgType&, SecondInArgType&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType, FirstInArgType, SecondInArgType>();
}
#endif

TEST(MethodHandlerCheckerNoReturnAndNoInArgsTest, CallingAssertCallableWithValidCallableDoesNotTerminate)
{
    // Given a method with no return type and no InArgs.
    using MethodReturnType = void;

    // and given a callable that accepts no return and no InArgs.
    using ValidCallable = std::function<void()>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the call does not terminate
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType>();
}

// TODO: Tests for invalid callables that should cause static assertion failures. These tests are disabled since we
// currently don't have infrastructure for compile time testing. To be enabled in SWP-46885.
#if 0
TEST(MethodHandlerCheckerNoReturnAndNoInArgsTest, CallingAssertCallableWithCallableWithInArgTerminates)
{
    // Given a method with no return type and no InArgs.
    using MethodReturnType = void;

    // and given a callable that accepts an InArg.
    using ValidCallable = std::function<void(const int&)>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType>();
}

TEST(MethodHandlerCheckerNoReturnAndNoInArgsTest, CallingAssertCallableWithCallableWithReturnTypeTerminates)
{
    // Given a method with no return type and no InArgs.
    using MethodReturnType = void;

    // and given a callable that returns a non-void value.
    using ValidCallable = std::function<int()>;

    // When calling AssertCallableMatchesMethodSignature
    // Then the function should not compile
    AssertCallableMatchesMethodSignature<ValidCallable, MethodReturnType>();
}
#endif

}  // namespace

}  // namespace score::mw::com::impl
