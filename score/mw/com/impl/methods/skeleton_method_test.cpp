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

#include "score/result/result.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_method.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/methods/skeleton_method.h"
#include "score/mw/com/impl/methods/skeleton_method_base.h"
#include "score/mw/com/impl/plumbing/skeleton_method_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_method_binding_factory_mock.h"
#include "score/mw/com/impl/skeleton_base.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstddef>
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
std::uint16_t kInstanceId{23U};
const ServiceInstanceDeployment kDeploymentInfo{kServiceIdentifier,
                                                LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
                                                QualityType::kASIL_QM,
                                                kInstanceSpecifier};
std::uint16_t kServiceId{34U};
const ServiceTypeDeployment kTypeDeployment{LolaServiceTypeDeployment{kServiceId}};
const auto kInstanceIdWithLolaBinding = make_InstanceIdentifier(kDeploymentInfo, kTypeDeployment);

class EmptySkeleton final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;
};

using TestMethodType = bool(int, bool);

class SkeletonMethodTestFixture : public ::testing::Test
{
  public:
    void CreateSkeletonMethod()
    {
        EmptySkeleton empty_skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
        auto mock_method_binding_ptr = std::make_unique<mock_binding::SkeletonMethodFacade>(mock_method_binding_);

        method_ = std::make_unique<SkeletonMethod<TestMethodType>>(
            empty_skeleton, "dummy_method", std::move(mock_method_binding_ptr));
    }

    std::unique_ptr<SkeletonMethod<TestMethodType>> method_{nullptr};
    mock_binding::SkeletonMethod mock_method_binding_{};
};

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
                  "Class type does not depend on event data type");
}

template <typename T>
class SkeletonMethodTypedTest : public ::testing::Test
{
  public:
    using Type = T;

    void SetUp() override
    {
        ON_CALL(skeleton_method_binding_mock_, RegisterHandler(_)).WillByDefault(Return(ResultBlank{}));
    }
    mock_binding::SkeletonMethod skeleton_method_binding_mock_;
};

struct MyDataStruct
{
    bool b;
    int i;
    double d;
    float f[4];
};
using RegisteredFunctionTypes = ::testing::Types<TestMethodType,
                                                 void(int),
                                                 void(const double, int),
                                                 void(char, MyDataStruct),
                                                 int(),
                                                 void(),
                                                 MyDataStruct(MyDataStruct, int, float)>;
TYPED_TEST_SUITE(SkeletonMethodTypedTest, RegisteredFunctionTypes, );

TYPED_TEST(SkeletonMethodTypedTest, AnyCombinationOfReturnAndInputArgTypesCanBeRegistered)
{

    // Given A skeleton Method with a mock method binding
    EmptySkeleton empty_skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
    auto mock_method_binding_ptr = std::make_unique<mock_binding::SkeletonMethod>();
    auto& mock_method_binding = *mock_method_binding_ptr;

    // And a method type that can be one of the four qualitatively different Types
    //  void()          SomeType()
    //  void(SomeTypes) SomeType(SomeTypes)
    using FixtureMethodType = typename TestFixture::Type;
    SkeletonMethod<FixtureMethodType> method{empty_skeleton, "dummy_method", std::move(mock_method_binding_ptr)};

    // Expecting that the register call is dispatched to the binding without errors
    EXPECT_CALL(mock_method_binding, RegisterHandler(_));

    // When a Register call is issued at the binding independent level
    score::cpp::callback<FixtureMethodType> test_callback{};

    method.RegisterHandler(std::move(test_callback));
}

TYPED_TEST(SkeletonMethodTypedTest, TwoParameterConstructorCorrectlyCallsBindingFactoryAndSkeletonMethodIsCreated)
{

    auto skeleton_method_binding_factory_mock = SkeletonMethodBindingFactoryMock();
    SkeletonMethodBindingFactory::InjectMockBinding(&skeleton_method_binding_factory_mock);

    auto skeleton_method_binding =
        std::make_unique<mock_binding::SkeletonMethodFacade>(this->skeleton_method_binding_mock_);

    // Given A skeleton Method with a mock method binding

    EmptySkeleton empty_skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // And a method type that can be one of the four qualitatively different Types void()          SomeType()
    //  void(SomeTypes) SomeType(SomeTypes)

    // expecting that a binding factory cannot crete a binding
    EXPECT_CALL(skeleton_method_binding_factory_mock, Create(_ /*handle*/, _ /*parent binding*/, _ /*method_name*/))
        .WillOnce(testing::Return(testing::ByMove(std::move(skeleton_method_binding))));

    // When the 2-parameter constructor of the SkeletonMethod class is called
    using FixtureMethodType = typename TestFixture::Type;
    SkeletonMethod<FixtureMethodType> method{empty_skeleton, "dummy_method"};

    EXPECT_CALL(this->skeleton_method_binding_mock_, RegisterHandler(_));

    // Then a Binding can be created which is capable of registering a callback
    score::cpp::callback<FixtureMethodType> test_callback{};
    method.RegisterHandler(std::move(test_callback));
}

TYPED_TEST(
    SkeletonMethodTypedTest,
    TwoParameterConstructorCorrectlyCallsBindingFactoryButSkeletonMethodIsNotCreatedWhenTheBindingFactoryDoesNotReturnBinding)
{

    auto skeleton_method_binding_factory_mock = SkeletonMethodBindingFactoryMock();
    SkeletonMethodBindingFactory::InjectMockBinding(&skeleton_method_binding_factory_mock);

    auto skeleton_method_binding =
        std::make_unique<mock_binding::SkeletonMethodFacade>(this->skeleton_method_binding_mock_);

    // Given A skeleton Method with a mock method binding

    EmptySkeleton empty_skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

    // And a method type that can be one of the four qualitatively different Types
    //  void()          SomeType()
    //  void(SomeTypes) SomeType(SomeTypes)

    // expecting that a binding factory cannot crete a binding
    EXPECT_CALL(skeleton_method_binding_factory_mock, Create(_ /*handle*/, _ /*parent binding*/, _ /*method_name*/))
        .WillOnce(testing::Return(testing::ByMove(nullptr)));

    // When the 2-parameter constructor of the SkeletonMethod class is called
    using FixtureMethodType = typename TestFixture::Type;
    SkeletonMethod<FixtureMethodType> method{empty_skeleton, "dummy_method"};

    // Then the binding cannot be created and calling AreBindingsValid returns false
    EXPECT_FALSE(SkeletonBaseView{empty_skeleton}.AreBindingsValid());
}

TEST_F(SkeletonMethodTestFixture, ACallbackWithAPointerAsStateCanBeRegistered)
{
    // Given A skeleton Method with a mock method binding
    CreateSkeletonMethod();

    // And a callback with a unique_ptr as a state
    auto test_struct_p = std::make_unique<MyDataStruct>();
    score::cpp::callback<TestMethodType> test_callback_with_state = [state = std::move(test_struct_p)](int, bool b) noexcept {
        return state->b || b;
    };

    // Expecting that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding_, RegisterHandler(_));

    // When a Register call is issued at the binding independent level
    method_->RegisterHandler(std::move(test_callback_with_state));
}

using Thing = long;
using InType1 = double;
using InType2 = int;
using VoidVoid = void();
using ThingVoid = Thing();
using VoidStuff = void(InType1, InType2);
using ThingStuff = Thing(InType1, InType2);

template <typename MethodType>
class SkeletonMethodGenericTestFixture : public ::testing::Test
{

  public:
    void CreateSkeletonMethodWithMockedTypeErasedCallback()
    {
        EmptySkeleton empty_skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};

        auto mock_method_binding_ptr = std::make_unique<mock_binding::SkeletonMethodFacade>(mock_method_binding_);
        method_ = std::make_unique<SkeletonMethod<MethodType>>(
            empty_skeleton, "dummy_method", std::move(mock_method_binding_ptr));
    }

    static constexpr std::size_t in_args_buffer_size = sizeof(InType1) + sizeof(InType2);
    void SerializeBuffers(InType1 in_arg_1, InType2 in_arg_2)
    {
        constexpr std::size_t in_type_1_size = sizeof(InType1);

        std::byte* write_head = in_args_buffer_.begin();
        new (write_head) InType1(in_arg_1);
        write_head += in_type_1_size;
        new (write_head) InType2(in_arg_2);
    }
    Thing GetTypedResultFromOutArgBuffer()
    {
        return *reinterpret_cast<Thing*>(out_arg_buffer_.data());
    }
    std::array<std::byte, sizeof(Thing)> out_arg_buffer_{};
    std::array<std::byte, in_args_buffer_size> in_args_buffer_{};

    std::unique_ptr<SkeletonMethod<MethodType>> method_{nullptr};
    ::testing::MockFunction<MethodType> typed_callback_mock_{};
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
        .WillOnce(Invoke([this](auto&& type_erased_callable) -> ResultBlank {
            typeerased_callback_.emplace(std::move(type_erased_callable));
            return {};
        }));

    Thing ret_val{505};
    InType1 in_arg_1{6.12};
    InType2 in_arg_2{17};

    // Expecting that a typed callable will be called with correctly deserialized inargs and will return a value
    EXPECT_CALL(typed_callback_mock_, Call(in_arg_1, in_arg_2)).WillOnce(Return(ret_val));

    SerializeBuffers(in_arg_1, in_arg_2);
    method_->RegisterHandler(typed_callback_mock_.AsStdFunction());
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
        .WillOnce(Invoke([this](auto&& type_erased_callable) -> ResultBlank {
            typeerased_callback_.emplace(std::move(type_erased_callable));
            return {};
        }));

    Thing ret_val{50255};

    // Expecting that a typed callable will be called without inargs and will return a value
    EXPECT_CALL(typed_callback_mock_, Call()).WillOnce(Return(ret_val));

    method_->RegisterHandler(typed_callback_mock_.AsStdFunction());

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
        .WillOnce(Invoke([this](auto&& type_erased_callable) -> ResultBlank {
            typeerased_callback_.emplace(std::move(type_erased_callable));
            return {};
        }));

    InType1 in_arg_1{0.6};
    InType2 in_arg_2{0x1700};

    // Expecting that a typed callable will be called with correctly deserialized inargs and will not return a value
    EXPECT_CALL(typed_callback_mock_, Call(in_arg_1, in_arg_2)).WillOnce(Return());

    SerializeBuffers(in_arg_1, in_arg_2);
    method_->RegisterHandler(typed_callback_mock_.AsStdFunction());

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
        .WillOnce(Invoke([this](auto&& type_erased_callable) -> ResultBlank {
            typeerased_callback_.emplace(std::move(type_erased_callable));
            return {};
        }));

    // Expecting that a typed callable will be called without inargs and will not return a value
    EXPECT_CALL(typed_callback_mock_, Call()).WillOnce(Return());

    method_->RegisterHandler(typed_callback_mock_.AsStdFunction());
    // When the type erased call is executed by the binding
    typeerased_callback_.value()({}, {});
}
}  // namespace

}  // namespace score::mw::com::impl
