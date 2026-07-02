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
#include "score/mw/com/impl/methods/callable_traits.h"
#include "score/mw/com/impl/methods/test/callable_traits_resources.h"

#include <score/callback.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <tuple>

namespace score::mw::com::impl
{

// Test data structures
struct MyDataStruct
{
    int i;
    std::string s;
    bool b;
};

template <typename Callable, typename ExpectedReturn>
struct ReturnTypeTestData
{
    using CallableType = Callable;
    using ExpectedReturnType = ExpectedReturn;
};

template <typename ReturnTypeTestData>
class CallableTraitsReturnTypeFixture : public ::testing::Test
{
  public:
    using CallableType = typename ReturnTypeTestData::CallableType;
    using ExpectedReturnType = typename ReturnTypeTestData::ExpectedReturnType;
};

using MyReturnTypeTestData = ::testing::Types<
    ReturnTypeTestData<score::cpp::callback<int()>, int>,
    ReturnTypeTestData<score::cpp::callback<MyDataStruct(double)>, MyDataStruct>,
    ReturnTypeTestData<score::cpp::callback<void(double)>, void>,
    ReturnTypeTestData<score::cpp::callback<void()>, void>,

    ReturnTypeTestData<std::function<int()>, int>,
    ReturnTypeTestData<std::function<MyDataStruct(double)>, MyDataStruct>,
    ReturnTypeTestData<std::function<void(double)>, void>,
    ReturnTypeTestData<std::function<void()>, void>,

    ReturnTypeTestData<DummyMethodFunctorConst<int()>, int>,
    ReturnTypeTestData<DummyMethodFunctorConst<MyDataStruct(double)>, MyDataStruct>,
    ReturnTypeTestData<DummyMethodFunctorConst<void(double)>, void>,
    ReturnTypeTestData<DummyMethodFunctorConst<void()>, void>,

    ReturnTypeTestData<DummyMethodFunctorConstNoexcept<int()>, int>,
    ReturnTypeTestData<DummyMethodFunctorConstNoexcept<MyDataStruct(double)>, MyDataStruct>,
    ReturnTypeTestData<DummyMethodFunctorConstNoexcept<void(double)>, void>,
    ReturnTypeTestData<DummyMethodFunctorConstNoexcept<void()>, void>,

    ReturnTypeTestData<DummyMethodFunctorNonConst<int()>, int>,
    ReturnTypeTestData<DummyMethodFunctorNonConst<MyDataStruct(double)>, MyDataStruct>,
    ReturnTypeTestData<DummyMethodFunctorNonConst<void(double)>, void>,
    ReturnTypeTestData<DummyMethodFunctorNonConst<void()>, void>,

    ReturnTypeTestData<DummyMethodFunctorNonConstNoexcept<int()>, int>,
    ReturnTypeTestData<DummyMethodFunctorNonConstNoexcept<MyDataStruct(double)>, MyDataStruct>,
    ReturnTypeTestData<DummyMethodFunctorNonConstNoexcept<void(double)>, void>,
    ReturnTypeTestData<DummyMethodFunctorNonConstNoexcept<void()>, void>,

    ReturnTypeTestData<decltype(NonNoexceptMethodLambdaFactory<int()>::Create()), int>,
    ReturnTypeTestData<decltype(NonNoexceptMethodLambdaFactory<MyDataStruct(double)>::Create()), MyDataStruct>,
    ReturnTypeTestData<decltype(NonNoexceptMethodLambdaFactory<void(double)>::Create()), void>,
    ReturnTypeTestData<decltype(NonNoexceptMethodLambdaFactory<void()>::Create()), void>,

    ReturnTypeTestData<decltype(NoexceptMethodLambdaFactory<int()>::Create()), int>,
    ReturnTypeTestData<decltype(NoexceptMethodLambdaFactory<MyDataStruct(double)>::Create()), MyDataStruct>,
    ReturnTypeTestData<decltype(NoexceptMethodLambdaFactory<void(double)>::Create()), void>,
    ReturnTypeTestData<decltype(NoexceptMethodLambdaFactory<void()>::Create()), void>,

    ReturnTypeTestData<decltype(MutableMethodLambdaFactory<int()>::Create()), int>,
    ReturnTypeTestData<decltype(MutableMethodLambdaFactory<MyDataStruct(double)>::Create()), MyDataStruct>,
    ReturnTypeTestData<decltype(MutableMethodLambdaFactory<void(double)>::Create()), void>,
    ReturnTypeTestData<decltype(MutableMethodLambdaFactory<void()>::Create()), void>,

    ReturnTypeTestData<decltype(MutableNoexceptMethodLambdaFactory<int()>::Create()), int>,
    ReturnTypeTestData<decltype(MutableNoexceptMethodLambdaFactory<MyDataStruct(double)>::Create()), MyDataStruct>,
    ReturnTypeTestData<decltype(MutableNoexceptMethodLambdaFactory<void(double)>::Create()), void>,
    ReturnTypeTestData<decltype(MutableNoexceptMethodLambdaFactory<void()>::Create()), void>

    >;
TYPED_TEST_SUITE(CallableTraitsReturnTypeFixture, MyReturnTypeTestData);

TYPED_TEST(CallableTraitsReturnTypeFixture, GetCallableReturnTypeForCallable)
{
    // When extracting the return type from a callable type
    using ActualReturnType = typename get_callable_return_type<typename TypeParam::CallableType>::type;

    // Then the extracted return type matches the expected return type
    EXPECT_TRUE((std::is_same_v<ActualReturnType, typename TypeParam::ExpectedReturnType>));
}

template <typename Callable, typename ExpectedInArgs>
struct InArgTypesTestData
{
    using CallableType = Callable;
    using ExpectedInArgTypes = ExpectedInArgs;
};

template <typename InArgTypesTestData>
class CallableTraitsInArgTypesFixture : public ::testing::Test
{
  public:
    using CallableType = typename InArgTypesTestData::CallableType;
    using ExpectedInArgTypes = typename InArgTypesTestData::ExpectedInArgTypes;
};

// clang-format off
using MyInArgTypesTestData = ::testing::Types<
    InArgTypesTestData<score::cpp::callback<void(int)>, std::tuple<int>>,
    InArgTypesTestData<score::cpp::callback<int(int)>, std::tuple<int>>,
    InArgTypesTestData<score::cpp::callback<void(const int, MyDataStruct&, const float&)>, std::tuple<int, MyDataStruct&, const float&>>,
    InArgTypesTestData<score::cpp::callback<void(const int, int*, const double*)>, std::tuple<int, int*, const double*>>,

    InArgTypesTestData<std::function<void(int)>, std::tuple<int>>,
    InArgTypesTestData<std::function<int(int)>, std::tuple<int>>,
    InArgTypesTestData<std::function<void(const int, MyDataStruct&, const float&)>, std::tuple<int, MyDataStruct&, const float&>>,
    InArgTypesTestData<std::function<void(const int, int*, const double*)>, std::tuple<int, int*, const double*>>,

    InArgTypesTestData<DummyMethodFunctorConst<void(int)>, std::tuple<int>>,
    InArgTypesTestData<DummyMethodFunctorConst<int(int)>, std::tuple<int>>,
    InArgTypesTestData<DummyMethodFunctorConst<void(const int, MyDataStruct&, const float&)>, std::tuple<int, MyDataStruct&, const float&>>,
    InArgTypesTestData<DummyMethodFunctorConst<void(const int, int*, const double*)>, std::tuple<int, int*, const double*>>,

    InArgTypesTestData<DummyMethodFunctorConstNoexcept<void(int)>, std::tuple<int>>,
    InArgTypesTestData<DummyMethodFunctorConstNoexcept<int(int)>, std::tuple<int>>,
    InArgTypesTestData<DummyMethodFunctorConstNoexcept<void(const int, int*, const double*)>, std::tuple<int, int*, const double*>>,
    InArgTypesTestData<DummyMethodFunctorConstNoexcept<void(const int, MyDataStruct&, const float&)>, std::tuple<int, MyDataStruct&, const float&>>,

    InArgTypesTestData<DummyMethodFunctorNonConst<void(int)>, std::tuple<int>>,
    InArgTypesTestData<DummyMethodFunctorNonConst<int(int)>, std::tuple<int>>,
    InArgTypesTestData<DummyMethodFunctorNonConst<void(const int, MyDataStruct&, const float&)>, std::tuple<int, MyDataStruct&, const float&>>,
    InArgTypesTestData<DummyMethodFunctorNonConst<void(const int, int*, const double*)>, std::tuple<int, int*, const double*>>,

    InArgTypesTestData<DummyMethodFunctorNonConstNoexcept<void(int)>, std::tuple<int>>,
    InArgTypesTestData<DummyMethodFunctorNonConstNoexcept<int(int)>, std::tuple<int>>,
    InArgTypesTestData<DummyMethodFunctorNonConstNoexcept<void(const int, MyDataStruct&, const float&)>, std::tuple<int, MyDataStruct&, const float&>>,
    InArgTypesTestData<DummyMethodFunctorNonConstNoexcept<void(const int, int*, const double*)>, std::tuple<int, int*, const double*>>,

    InArgTypesTestData<decltype(NonNoexceptMethodLambdaFactory<void(int)>::Create()), std::tuple<int>>,
    InArgTypesTestData<decltype(NonNoexceptMethodLambdaFactory<int(int)>::Create()), std::tuple<int>>,
    InArgTypesTestData<decltype(NonNoexceptMethodLambdaFactory<void(const int, MyDataStruct&, const float&)>::Create()), std::tuple<int, MyDataStruct&, const float&>>,
    InArgTypesTestData<decltype(NonNoexceptMethodLambdaFactory<void(const int, int*, const double*)>::Create()), std::tuple<int, int*, const double*>>,

    InArgTypesTestData<decltype(NoexceptMethodLambdaFactory<void(int)>::Create()), std::tuple<int>>,
    InArgTypesTestData<decltype(NoexceptMethodLambdaFactory<int(int)>::Create()), std::tuple<int>>,
    InArgTypesTestData<decltype(NoexceptMethodLambdaFactory<void(const int, MyDataStruct&, const float&)>::Create()), std::tuple<int, MyDataStruct&, const float&>>,
    InArgTypesTestData<decltype(NoexceptMethodLambdaFactory<void(const int, int*, const double*)>::Create()), std::tuple<int, int*, const double*>>,

    InArgTypesTestData<decltype(MutableMethodLambdaFactory<void(int)>::Create()), std::tuple<int>>,
    InArgTypesTestData<decltype(MutableMethodLambdaFactory<int(int)>::Create()), std::tuple<int>>,
    InArgTypesTestData<decltype(MutableMethodLambdaFactory<void(const int, MyDataStruct&, const float&)>::Create()), std::tuple<int, MyDataStruct&, const float&>>,
    InArgTypesTestData<decltype(MutableMethodLambdaFactory<void(const int, int*, const double*)>::Create()), std::tuple<int, int*, const double*>>,

    InArgTypesTestData<decltype(MutableNoexceptMethodLambdaFactory<void(int)>::Create()), std::tuple<int>>,
    InArgTypesTestData<decltype(MutableNoexceptMethodLambdaFactory<int(int)>::Create()), std::tuple<int>>,
    InArgTypesTestData<decltype(MutableNoexceptMethodLambdaFactory<void(const int, MyDataStruct&, const float&)>::Create()), std::tuple<int, MyDataStruct&, const float&>>,
    InArgTypesTestData<decltype(MutableNoexceptMethodLambdaFactory<void(const int, int*, const double*)>::Create()), std::tuple<int, int*, const double*>>
    >;
// clang-format on

TYPED_TEST_SUITE(CallableTraitsInArgTypesFixture, MyInArgTypesTestData);

TYPED_TEST(CallableTraitsInArgTypesFixture, GetCallableInArgTypesForCallable)
{
    // When extracting the InArg types from a callable type
    using ActualInArgTypes = typename get_callable_args_types<typename TypeParam::CallableType>::type;

    // Then the extracted InArg types match the expected InArg types
    EXPECT_TRUE((std::is_same_v<ActualInArgTypes, typename TypeParam::ExpectedInArgTypes>));
}

TEST(CallableTraitsIsTupleEmptyTest, EmptyTupleReturnsTrue)
{
    // When checking if a tuple containg no types is empty
    // Then the result is true
    EXPECT_TRUE(is_tuple_empty<std::tuple<>>::value);
}

TEST(CallableTraitsIsTupleEmptyTest, TupleWithOneArgReturnsFalse)
{
    // When checking if a tuple containg one type is empty
    // Then the result is false
    EXPECT_FALSE(is_tuple_empty<std::tuple<int>>::value);
}

TEST(CallableTraitsIsTupleEmptyTest, TupleWithMultipleArgsReturnsFalse)
{
    // When checking if a tuple containg multiple types is empty
    // Then the result is false
    EXPECT_FALSE((is_tuple_empty<std::tuple<int, double, std::string>>::value));
}

TEST(CallableTraitsGetTupleTailTest, GetTupleTailOfSingleElementTupleReturnsEmptyTuple)
{
    // When extracting the tail of a tuple containing one type
    using Tail = typename get_tuple_tail<std::tuple<int>>::type;

    // Then the extracted tail is an empty tuple
    EXPECT_TRUE((std::is_same_v<Tail, std::tuple<>>));
}

TEST(CallableTraitsGetTupleTailTest, GetTupleTailOfMultiElementTupleReturnsTail)
{
    // When extracting the tail of a tuple containing multiple types
    using Tail = typename get_tuple_tail<std::tuple<int, double, std::string>>::type;

    // Then the extracted tail is a tuple containing all types except the first one
    EXPECT_TRUE((std::is_same_v<Tail, std::tuple<double, std::string>>));
}

// TODO: Tests for invalid callables that should cause static assertion failures. These tests are disabled since we
// currently don't have infrastructure for compile time testing. To be enabled in SWP-46885.
#if 0
TEST(CallableTraitsGetTupleTailTest, GetTupleTailOfEmptyTupleFailsToCompile)
{
    // When extracting the tail of an empty tuple
    // Then we should get a compiler error
    using Tail = typename get_tuple_tail<std::tuple<>>::type;
}
#endif

}  // namespace score::mw::com::impl
