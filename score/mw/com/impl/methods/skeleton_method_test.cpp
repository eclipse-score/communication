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
#include "score/mw/com/impl/methods/skeleton_method.h"

#include "gmock/gmock.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_method.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/methods/skeleton_method.h"
#include "score/mw/com/impl/methods/skeleton_method_base.h"
#include "score/mw/com/impl/methods/test/callable_traits_resources.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/result/result.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstddef>
#include <functional>
#include <memory>

#include <score/callback.hpp>

namespace score::mw::com::impl
{

namespace
{
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
const auto kServiceIdentifier = make_ServiceIdentifierType("foo", 13, 37);
constexpr std::uint16_t kInstanceId{23U};
const ServiceInstanceDeployment kDeploymentInfo{kServiceIdentifier,
                                                LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
                                                QualityType::kASIL_QM,
                                                kInstanceSpecifier};
constexpr std::uint16_t kServiceId{34U};
const ServiceTypeDeployment kTypeDeployment{LolaServiceTypeDeployment{kServiceId}};
const auto kInstanceIdWithLolaBinding = make_InstanceIdentifier(kDeploymentInfo, kTypeDeployment);

class EmptySkeleton final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;
};

using TestMethodType = bool(const int, const bool);
using TestMethodHandlerType = void(bool&, const int&, const bool&);

TEST(SkeletonMethodTests, NotCopyable)
{
    static_assert(!std::is_copy_constructible<SkeletonMethod<TestMethodType>>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<SkeletonMethod<TestMethodType>>::value, "Is wrongly copyable");
}

TEST(SkeletonMethodTests, IsMoveable)
{
    static_assert(std::is_move_constructible<SkeletonMethod<TestMethodType>>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<SkeletonMethod<TestMethodType>>::value, "Is not move assignable");
}

TEST(SkeletonMethodTest, SkeletonMethodContainsPublicMethodType)
{
    static_assert(std::is_same<SkeletonMethod<TestMethodType>::MethodType, TestMethodType>::value,
                  "Incorrect CallbackType.");
}

TEST(SkeletonMethodTest, ClassTypeDependsOnMethodType)
{
    using FirstSkeletonMethodType = SkeletonMethod<bool(int)>;
    using SecondSkeletonMethodType = SkeletonMethod<void(std::uint16_t)>;
    static_assert(!std::is_same_v<FirstSkeletonMethodType, SecondSkeletonMethodType>,
                  "Class type does not depend on method signature.");
}

struct MyDataStruct
{
    bool b;
    int i;
    double d;
    float f[4];
};

template <typename MethodType>
class SkeletonMethodFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(mock_method_binding_, RegisterHandler(_)).WillByDefault(Return(Result<void>{}));
    }

    SkeletonMethodFixture& GivenASkeletonMethod()
    {
        auto mock_method_binding_ptr = std::make_unique<mock_binding::SkeletonMethodFacade>(mock_method_binding_);

        method_ = std::make_unique<SkeletonMethod<MethodType>>(
            empty_skeleton_, method_name_, std::move(mock_method_binding_ptr));
        return *this;
    }

    SkeletonMethodFixture& WithAMockedMethodBindingfactory()
    {
        skeleton_method_binding_factory_mock_guard_ = std::make_unique<SkeletonMethodBindingFactoryMockGuard>();
        return *this;
    }

    EmptySkeleton empty_skeleton_{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
    const std::string method_name_{"dummy_method"};
    std::unique_ptr<SkeletonMethod<MethodType>> method_{nullptr};
    mock_binding::SkeletonMethod mock_method_binding_{};

    std::unique_ptr<SkeletonMethodBindingFactoryMockGuard> skeleton_method_binding_factory_mock_guard_{nullptr};
};

template <typename MethodHandlerFactoryIn, typename MethodTypeIn>
struct FactoryAndMethodType
{
    using MethodHandlerFactory = MethodHandlerFactoryIn;
    using MethodType = MethodTypeIn;
};

template <typename TestTypes>
class SkeletonMethodTypedFixture : public SkeletonMethodFixture<typename TestTypes::MethodType>
{
  public:
    using MethodHandlerFactory = typename TestTypes::MethodHandlerFactory;
    using MethodType = typename TestTypes::MethodType;

    auto CreateHandler()
    {
        return MethodHandlerFactory::Create();
    }
};

// clang-format off
using RegisteredFunctionTypes = ::testing::Types<
    FactoryAndMethodType<CallableFactory<score::cpp::callback, void(int&)>, int()>,
    FactoryAndMethodType<CallableFactory<score::cpp::callback, void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<CallableFactory<score::cpp::callback, void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<CallableFactory<score::cpp::callback, void()>, void()>,

    FactoryAndMethodType<CallableFactory<std::function, void(int&)>, int()>,
    FactoryAndMethodType<CallableFactory<std::function, void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<CallableFactory<std::function, void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<CallableFactory<std::function, void()>, void()>,

    FactoryAndMethodType<CallableFactory<DummyMethodFunctorConst, void(int&)>, int()>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorConst, void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorConst, void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorConst, void()>, void()>,

    FactoryAndMethodType<CallableFactory<DummyMethodFunctorConstNoexcept, void(int&)>, int()>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorConstNoexcept, void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorConstNoexcept, void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorConstNoexcept, void()>, void()>,

    FactoryAndMethodType<CallableFactory<DummyMethodFunctorNonConst, void(int&)>, int()>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorNonConst, void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorNonConst, void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorNonConst, void()>, void()>,

    FactoryAndMethodType<CallableFactory<DummyMethodFunctorNonConstNoexcept, void(int&)>, int()>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorNonConstNoexcept, void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorNonConstNoexcept, void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<CallableFactory<DummyMethodFunctorNonConstNoexcept, void()>, void()>,

    FactoryAndMethodType<NonNoexceptMethodLambdaFactory<void(int&)>, int()>,
    FactoryAndMethodType<NonNoexceptMethodLambdaFactory<void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<NonNoexceptMethodLambdaFactory<void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<NonNoexceptMethodLambdaFactory<void()>, void()>,

    FactoryAndMethodType<NoexceptMethodLambdaFactory<void(int&)>, int()>,
    FactoryAndMethodType<NoexceptMethodLambdaFactory<void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<NoexceptMethodLambdaFactory<void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<NoexceptMethodLambdaFactory<void()>, void()>,

    FactoryAndMethodType<MutableMethodLambdaFactory<void(int&)>, int()>,
    FactoryAndMethodType<MutableMethodLambdaFactory<void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<MutableMethodLambdaFactory<void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<MutableMethodLambdaFactory<void()>, void()>,

    FactoryAndMethodType<MutableNoexceptMethodLambdaFactory<void(int&)>, int()>,
    FactoryAndMethodType<MutableNoexceptMethodLambdaFactory<void(float&, const double&, const bool&)>, float(double, bool)>,
    FactoryAndMethodType<MutableNoexceptMethodLambdaFactory<void(const MyDataStruct&, const bool&)>, void(MyDataStruct, bool)>,
    FactoryAndMethodType<MutableNoexceptMethodLambdaFactory<void()>, void()>

>;
// clang-format on

TYPED_TEST_SUITE(SkeletonMethodTypedFixture, RegisteredFunctionTypes, );

TYPED_TEST(SkeletonMethodTypedFixture, AnyCombinationOfReturnAndInputArgTypesCanBeRegistered)
{
    // Given A skeleton Method with a mock method binding
    this->GivenASkeletonMethod();

    // Expecting that the register call is dispatched to the binding without errors
    EXPECT_CALL(this->mock_method_binding_, RegisterHandler(_));

    // When a Register call is issued at the binding independent level
    auto test_callback = this->CreateHandler();
    std::ignore = this->method_->RegisterHandler(std::move(test_callback));
}

TYPED_TEST(SkeletonMethodTypedFixture, TwoParameterConstructorCorrectlyCallsBindingFactoryAndSkeletonMethodIsCreated)
{
    this->WithAMockedMethodBindingfactory();

    // Expecting that a binding factory can create a binding
    EXPECT_CALL(this->skeleton_method_binding_factory_mock_guard_->factory_mock_,
                Create(_ /*handle*/, _ /*parent binding*/, this->method_name_, _))
        .WillOnce(testing::Return(
            testing::ByMove(std::make_unique<mock_binding::SkeletonMethodFacade>(this->mock_method_binding_))));

    // When the 2-parameter constructor of the SkeletonMethod class is called
    using FixtureMethodType = typename TestFixture::MethodType;
    SkeletonMethod<FixtureMethodType> method{this->empty_skeleton_, this->method_name_};

    // Then a Binding can be created which is capable of registering a callback
    auto test_callback = this->CreateHandler();
    EXPECT_TRUE(method.RegisterHandler(std::move(test_callback)));
}

TYPED_TEST(SkeletonMethodTypedFixture, TwoParameterConstructorMarksBindingsAsInvalidWhenFactoryReturnsNullptr)
{
    this->WithAMockedMethodBindingfactory();

    // Expecting that a binding factory cannot create a binding
    EXPECT_CALL(this->skeleton_method_binding_factory_mock_guard_->factory_mock_,
                Create(_ /*handle*/, _ /*parent binding*/, this->method_name_, _ /*method_type*/))
        .WillOnce(testing::Return(testing::ByMove(nullptr)));

    // When the 2-parameter constructor of the SkeletonMethod class is called
    using FixtureMethodType = typename TestFixture::MethodType;
    SkeletonMethod<FixtureMethodType> method{this->empty_skeleton_, this->method_name_};

    // Then the binding cannot be created and calling AreBindingsValid returns false
    EXPECT_FALSE(SkeletonBaseView{this->empty_skeleton_}.AreBindingsValid());
}

using SkeletonMethodStatefulCallbackFixture = SkeletonMethodFixture<void()>;
TEST_F(SkeletonMethodStatefulCallbackFixture, ACallbackWithAPointerAsStateCanBeRegistered)
{
    // Given A skeleton Method with a mock method binding
    GivenASkeletonMethod();

    // And a callback with a unique_ptr as a state
    auto test_struct_p = std::make_unique<MyDataStruct>();
    score::cpp::callback<void()> test_callback_with_state = [state = std::move(test_struct_p)]() noexcept {};

    // Expecting that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding_, RegisterHandler(_));

    // When a Register call is issued at the binding independent level
    std::ignore = method_->RegisterHandler(std::move(test_callback_with_state));
}

TEST_F(SkeletonMethodStatefulCallbackFixture, PassingReferenceToHandlerUpdatesStateInPlace)
{
    static constexpr int kInitialValue = 42;
    static constexpr int kModifiedValue = 43;

    // Given A skeleton Method with a mock method binding
    GivenASkeletonMethod();

    // And a functor which modifies its internal state when called
    class DummyMethodFunctor
    {
      public:
        void operator()()
        {
            i_ = kModifiedValue;
        }

        int i_{kInitialValue};
    };
    DummyMethodFunctor test_functor{};

    // Expecting that the register call is dispatched to the binding which captures the handler
    std::optional<SkeletonMethodBinding::TypeErasedHandler> captured_set_handler{};
    EXPECT_CALL(mock_method_binding_, RegisterHandler(_)).WillOnce(Invoke([&captured_set_handler](auto handler) {
        captured_set_handler = std::move(handler);
        return Result<void>{};
    }));

    // and given a Register call is issued at the binding independent level with an lvalue reference to the functor
    std::ignore = method_->RegisterHandler(test_functor);

    // When the type erased call is executed by the binding
    captured_set_handler.value()({}, {});

    // Then the state of the functor is updated in place when the handler is called by the binding
    EXPECT_EQ(test_functor.i_, kModifiedValue);
}

template <typename HandlerTypeIn, typename MethodTypeIn>
struct MethodTypeAndHandlerType
{
    using HandlerType = HandlerTypeIn;
    using MethodType = MethodTypeIn;
};

using Thing = long;
using InType1 = double;
using InType2 = int;
using VoidVoid = MethodTypeAndHandlerType<void(), void()>;
using ThingVoid = MethodTypeAndHandlerType<void(Thing&), Thing()>;
using VoidStuff =
    MethodTypeAndHandlerType<void(const InType1&, const InType2&, const InType2&), void(InType1, InType2, InType2)>;
using ThingStuff = MethodTypeAndHandlerType<void(Thing&, const InType1&, const InType2&, const InType2&),
                                            Thing(InType1, InType2, InType2)>;

template <typename Types>
class SkeletonMethodGenericTestFixture : public ::testing::Test
{
  public:
    static constexpr std::size_t in_args_buffer_size = sizeof(InType1) + sizeof(InType2) + sizeof(InType2);

    void CreateSkeletonMethodWithMockedTypeErasedCallback()
    {
        EmptySkeleton empty_skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

        auto mock_method_binding_ptr = std::make_unique<mock_binding::SkeletonMethodFacade>(mock_method_binding_);
        method_ = std::make_unique<SkeletonMethod<typename Types::MethodType>>(
            empty_skeleton, "dummy_method", std::move(mock_method_binding_ptr));
    }

    void SerializeBuffers(InType1 in_arg_1, InType2 in_arg_2, InType2 in_arg_3)
    {
        constexpr std::size_t in_type_1_size = sizeof(InType1);
        constexpr std::size_t in_type_2_size = sizeof(InType2);

        std::byte* write_head = in_args_buffer_.begin();
        new (write_head) InType1(in_arg_1);
        write_head += in_type_1_size;
        new (write_head) InType2(in_arg_2);
        write_head += in_type_2_size;
        new (write_head) InType2(in_arg_3);
    }
    Thing GetTypedResultFromOutArgBuffer()
    {
        return *reinterpret_cast<Thing*>(out_arg_buffer_.data());
    }
    std::array<std::byte, sizeof(Thing)> out_arg_buffer_{};
    std::array<std::byte, in_args_buffer_size> in_args_buffer_{};

    std::unique_ptr<SkeletonMethod<typename Types::MethodType>> method_{nullptr};
    ::testing::MockFunction<typename Types::HandlerType> typed_callback_mock_{};
    std::optional<SkeletonMethodBinding::TypeErasedHandler> typeerased_callback_{};
    mock_binding::SkeletonMethod mock_method_binding_{};
};

using SkeletonMethodThingStuffFixture = SkeletonMethodGenericTestFixture<ThingStuff>;
TEST_F(SkeletonMethodThingStuffFixture, DataTransferBetweenTypedAndTypeErasedCallbacks)
{
    // Given A skeleton Method with a mock method binding
    CreateSkeletonMethodWithMockedTypeErasedCallback();

    // Expecting that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding_, RegisterHandler(_))
        .WillOnce(Invoke([this](auto&& type_erased_callable) -> Result<void> {
            typeerased_callback_.emplace(std::move(type_erased_callable));
            return {};
        }));

    Thing ret_val{505};
    InType1 in_arg_1{6.12};
    InType2 in_arg_2{17};
    InType2 in_arg_3{18};

    // Expecting that a typed callable will be called with correctly deserialized inargs and will return value
    EXPECT_CALL(typed_callback_mock_, Call(_, in_arg_1, in_arg_2, in_arg_3))
        .WillOnce(Invoke([ret_val](auto& return_arg, auto, auto, auto) {
            return_arg = ret_val;
        }));

    SerializeBuffers(in_arg_1, in_arg_2, in_arg_3);
    EXPECT_TRUE(method_->RegisterHandler(typed_callback_mock_.AsStdFunction()));
    // When the type erased call is executed by the binding
    typeerased_callback_.value()(in_args_buffer_, out_arg_buffer_);

    // Then its return is deserialized to the correct return value of the typed callback
    auto res = GetTypedResultFromOutArgBuffer();
    EXPECT_EQ(res, ret_val);
}

using SkeletonMethodThingVoidFixture = SkeletonMethodGenericTestFixture<ThingVoid>;
TEST_F(SkeletonMethodThingVoidFixture, DataTransferBetweenTypedAndTypeErasedCallbacks)
{
    // Given A skeleton Method with a mock method binding
    CreateSkeletonMethodWithMockedTypeErasedCallback();

    // Expecting that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding_, RegisterHandler(_))
        .WillOnce(Invoke([this](auto&& type_erased_callable) -> Result<void> {
            typeerased_callback_.emplace(std::move(type_erased_callable));
            return {};
        }));

    Thing ret_val{50255};

    // Expecting that a typed callable will be called without inargs and will return a value
    EXPECT_CALL(typed_callback_mock_, Call(_)).WillOnce(Invoke([ret_val](auto& return_arg) {
        return_arg = ret_val;
    }));

    EXPECT_TRUE(method_->RegisterHandler(typed_callback_mock_.AsStdFunction()));

    // When the type erased call is executed by the binding
    typeerased_callback_.value()(std::nullopt, out_arg_buffer_);

    // Then its return is deserialized to the correct return value of the typed callback
    auto res = GetTypedResultFromOutArgBuffer();
    EXPECT_EQ(res, ret_val);
}

using SkeletonMethodVoidStuffFixture = SkeletonMethodGenericTestFixture<VoidStuff>;
TEST_F(SkeletonMethodVoidStuffFixture, DataTransferBetweenTypedAndTypeErasedCallbacks)
{
    // Given A skeleton Method with a mock method binding
    CreateSkeletonMethodWithMockedTypeErasedCallback();

    // Expecting that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding_, RegisterHandler(_))
        .WillOnce(Invoke([this](auto&& type_erased_callable) -> Result<void> {
            typeerased_callback_.emplace(std::move(type_erased_callable));
            return {};
        }));

    InType1 in_arg_1{0.6};
    InType2 in_arg_2{0x1700};
    InType2 in_arg_3{0x1701};

    // Expecting that a typed callable will be called with correctly deserialized inargs and will not return a value
    EXPECT_CALL(typed_callback_mock_, Call(in_arg_1, in_arg_2, in_arg_3));

    SerializeBuffers(in_arg_1, in_arg_2, in_arg_3);
    EXPECT_TRUE(method_->RegisterHandler(typed_callback_mock_.AsStdFunction()));

    // When the type erased call is executed by the binding
    typeerased_callback_.value()(in_args_buffer_, {});
}

using SkeletonMethodVoidVoidFixture = SkeletonMethodGenericTestFixture<VoidVoid>;
TEST_F(SkeletonMethodVoidVoidFixture, DataTransferBetweenTypedAndTypeErasedCallbacks)
{
    // Given A skeleton Method with a mock method binding
    CreateSkeletonMethodWithMockedTypeErasedCallback();

    // Expecting that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding_, RegisterHandler(_))
        .WillOnce(Invoke([this](auto&& type_erased_callable) -> Result<void> {
            typeerased_callback_.emplace(std::move(type_erased_callable));
            return {};
        }));

    // Expecting that a typed callable will be called without inargs and will not return a value
    EXPECT_CALL(typed_callback_mock_, Call());

    EXPECT_TRUE(method_->RegisterHandler(typed_callback_mock_.AsStdFunction()));
    // When the type erased call is executed by the binding
    typeerased_callback_.value()({}, {});
}

}  // namespace
}  // namespace score::mw::com::impl
