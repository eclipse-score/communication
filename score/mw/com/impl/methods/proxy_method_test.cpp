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
#include "score/mw/com/impl/methods/proxy_method.h"
#include "score/mw/com/impl/methods/proxy_method_with_in_args.h"
#include "score/mw/com/impl/methods/proxy_method_with_in_args_and_return.h"
#include "score/mw/com/impl/methods/proxy_method_with_return_type.h"
#include "score/mw/com/impl/methods/proxy_method_without_in_args_or_return.h"

#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_method.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/result/result.h"

#include <score/assert_support.hpp>
#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <tuple>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

const std::string_view kMethodName{"DummyMethod"};

constexpr int kDummyArg1{42};
constexpr double kDummyArg2{3.14};
constexpr char kDummyArg3{'a'};

template <typename ProxyMethodType>
class ProxyMethodTestFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(proxy_method_binding_mock_, AllocateInArgs(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{method_in_args_buffer_.data(), method_in_args_buffer_.size()}}));
        ON_CALL(proxy_method_binding_mock_, AllocateReturnType(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{method_return_type_buffer_.data(), method_return_type_buffer_.size()}}));
    }

    ProxyMethodTestFixture& GivenAValidProxyMethod()
    {
        unit_ = std::make_unique<ProxyMethodType>(
            proxy_base_, std::make_unique<mock_binding::ProxyMethodFacade>(proxy_method_binding_mock_), kMethodName);
        return *this;
    }

    alignas(8) std::array<std::byte, 1024> method_in_args_buffer_{};
    alignas(8) std::array<std::byte, 1024> method_return_type_buffer_{};
    ConfigurationStore config_store_{InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value(),
                                     make_ServiceIdentifierType("foo"),
                                     QualityType::kASIL_QM,
                                     LolaServiceTypeDeployment{42U},
                                     LolaServiceInstanceDeployment{1U}};

    mock_binding::ProxyMethod proxy_method_binding_mock_;
    ProxyBase proxy_base_{std::make_unique<mock_binding::Proxy>(), config_store_.GetHandle()};

    std::unique_ptr<ProxyMethodType> unit_{nullptr};
};

using ProxyMethodWithInArgsAndReturn = ProxyMethod<bool(int, double, char)>;
using ProxyMethodWithInArgsOnly = ProxyMethod<void(int, double, char)>;
using ProxyMethodWithReturnOnly = ProxyMethod<int()>;
using ProxyMethodWithNoInArgsOrReturn = ProxyMethod<void()>;
using AllArgCombinations = ::testing::Types<ProxyMethodWithInArgsAndReturn,
                                            ProxyMethodWithInArgsOnly,
                                            ProxyMethodWithReturnOnly,
                                            ProxyMethodWithNoInArgsOrReturn>;
using WithInArgs = ::testing::Types<ProxyMethodWithInArgsAndReturn, ProxyMethodWithInArgsOnly>;
using WithOutInArgs = ::testing::Types<ProxyMethodWithReturnOnly, ProxyMethodWithNoInArgsOrReturn>;
using WithResult = ::testing::Types<ProxyMethodWithInArgsAndReturn, ProxyMethodWithReturnOnly>;

template <typename T>
using ProxyMethodAllArgCombinationsTestFixture = ProxyMethodTestFixture<T>;
TYPED_TEST_SUITE(ProxyMethodAllArgCombinationsTestFixture, AllArgCombinations, );

template <typename T>
using ProxyMethodWithInArgsTestFixture = ProxyMethodTestFixture<T>;
TYPED_TEST_SUITE(ProxyMethodWithInArgsTestFixture, WithInArgs, );

template <typename T>
using ProxyMethodWithoutInArgsTestFixture = ProxyMethodTestFixture<T>;
TYPED_TEST_SUITE(ProxyMethodWithoutInArgsTestFixture, WithOutInArgs, );

template <typename T>
using ProxyMethodWithResultTestFixture = ProxyMethodTestFixture<T>;
TYPED_TEST_SUITE(ProxyMethodWithResultTestFixture, WithResult, );

using ProxyMethodWithInArgsAndReturnFixture = ProxyMethodTestFixture<ProxyMethodWithInArgsAndReturn>;
using ProxyMethodWithReturnOnlyFixture = ProxyMethodTestFixture<ProxyMethodWithReturnOnly>;
using ProxyMethodWithNoInArgsOrReturnFixture = ProxyMethodTestFixture<ProxyMethodWithNoInArgsOrReturn>;
using ProxyMethodWithInArgsOnlyFixture = ProxyMethodTestFixture<ProxyMethodWithInArgsOnly>;

TYPED_TEST(ProxyMethodAllArgCombinationsTestFixture, Construction)
{
    // When constructing a ProxyMethod with all combinations of InArgs / return types
    // Then the ProxyMethod can be constructed
    using ProxyMethodType = TypeParam;
    ProxyMethodType{this->proxy_base_,
                    std::make_unique<mock_binding::ProxyMethodFacade>(this->proxy_method_binding_mock_),
                    kMethodName};
}

TYPED_TEST(ProxyMethodWithInArgsTestFixture, AllocateInArgs_ReturnsInArgPointersPointingToQueuePositionZero)
{
    this->GivenAValidProxyMethod();

    // Expecting that AllocateInArgs is called once for queue position 0 on the binding mock
    EXPECT_CALL(this->proxy_method_binding_mock_, AllocateInArgs(0U));

    // When Allocate is called on the ProxyMethod
    auto method_in_arg_ptr_tuple = this->unit_->Allocate();

    // Then a valid tuple of MethodInArgPtrs is returned
    ASSERT_TRUE(method_in_arg_ptr_tuple.has_value());

    // and the first MethodInArgPtr points to queue position 0 of the buffer
    auto& pointer0 = std::get<0>(method_in_arg_ptr_tuple.value());
    EXPECT_EQ(pointer0.GetQueuePosition(), 0);

    // and the second MethodInArgPtr points to queue position 0 of the buffer
    auto& pointer1 = std::get<1>(method_in_arg_ptr_tuple.value());
    EXPECT_EQ(pointer1.GetQueuePosition(), 0);

    // and the third MethodInArgPtr points to queue position 0 of the buffer
    auto& pointer2 = std::get<2>(method_in_arg_ptr_tuple.value());
    EXPECT_EQ(pointer2.GetQueuePosition(), 0);
}

TYPED_TEST(ProxyMethodWithInArgsTestFixture, AllocateInArgs_ReturnsInArgPointersPointingToInArgsAllocatedByBinding)
{
    auto* const buffer_start_address = &(this->method_in_args_buffer_[0]);
    const auto method_in_args_buffer_size = std::tuple_size<decltype(this->method_in_args_buffer_)>{};
    auto* const buffer_end_address =
        memory::shared::AddOffsetToPointer(buffer_start_address, method_in_args_buffer_size);

    this->GivenAValidProxyMethod();

    // Expecting that AllocateInArgs is called once for queue position 0 on the binding mock and returns a pointer to
    // our buffer
    EXPECT_CALL(this->proxy_method_binding_mock_, AllocateInArgs(0U));

    // When Allocate is called on the ProxyMethod
    auto method_in_arg_ptr_tuple = this->unit_->Allocate();

    // Then a valid tuple of MethodInArgPtrs is returned
    ASSERT_TRUE(method_in_arg_ptr_tuple.has_value());

    // and the first MethodInArgPtr points to the start of the buffer returned by the AllocateInArgs
    auto& pointer0 = std::get<0>(method_in_arg_ptr_tuple.value());
    auto* const pointed_to_address_0 = reinterpret_cast<std::byte*>(pointer0.get());
    EXPECT_EQ(pointed_to_address_0, buffer_start_address);

    // and the second MethodInArgPtr points to an address in the buffer after the address pointed to by the first
    // MethodInArgPtr (detailed position is not checked in TypeErasedCallQueue tests)
    auto& pointer1 = std::get<1>(method_in_arg_ptr_tuple.value());
    auto* const pointed_to_address_1 = reinterpret_cast<std::byte*>(pointer1.get());
    EXPECT_GT(pointed_to_address_1, pointed_to_address_0);
    EXPECT_LT(pointed_to_address_1, buffer_end_address);

    // and the third MethodInArgPtr points to an address in the buffer after the address pointed to by the second
    // MethodInArgPtr
    auto& pointer2 = std::get<2>(method_in_arg_ptr_tuple.value());
    auto* const pointed_to_address_2 = reinterpret_cast<std::byte*>(pointer2.get());
    EXPECT_GT(pointed_to_address_2, pointed_to_address_1);
    EXPECT_LT(pointed_to_address_2, buffer_end_address);
}

TYPED_TEST(ProxyMethodWithInArgsTestFixture, AllocateInArgs_QueueFullError)
{
    this->GivenAValidProxyMethod();

    // and given that Allocate was called once
    auto method_in_arg_ptr_tuple = this->unit_->Allocate();
    ASSERT_TRUE(method_in_arg_ptr_tuple.has_value());

    // when Allocate is called a 2nd time on the ProxyMethod (while still holding the method in arg pointers from the
    // 1st call)
    auto method_in_arg_ptr_tuple_2 = this->unit_->Allocate();

    // Then a CallQueueFull error is returned
    ASSERT_FALSE(method_in_arg_ptr_tuple_2.has_value());
    EXPECT_EQ(method_in_arg_ptr_tuple_2.error(), ComErrc::kCallQueueFull);
}

TYPED_TEST(ProxyMethodWithInArgsTestFixture, AllocateInArgs_BindingError)
{
    this->GivenAValidProxyMethod();

    // Expect that AllocateInArgs is called once for queue position 0 on the binding mock and returns an error.
    EXPECT_CALL(this->proxy_method_binding_mock_, AllocateInArgs(0U))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    // When calling Allocate()
    // Then the program terminates (An allocation failure from the binding is unexpected here as we already checked for
    // available queue slots on binding independent level)
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->unit_->Allocate());
}

TYPED_TEST(ProxyMethodWithInArgsTestFixture, CallOperator_WithCopy)
{
    this->GivenAValidProxyMethod();

    // Expecting that DoCall will be called on the binding
    EXPECT_CALL(this->proxy_method_binding_mock_, DoCall(0U));

    // When call operator is called on the ProxyMethod
    auto& proxy_method = *(this->unit_);
    auto call_result = proxy_method(kDummyArg1, kDummyArg2, kDummyArg3);

    // Then no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TYPED_TEST(ProxyMethodWithInArgsTestFixture, CallOperator_WithCopyTemporary)
{
    this->GivenAValidProxyMethod();

    // Expecting that DoCall will be called on the binding
    EXPECT_CALL(this->proxy_method_binding_mock_, DoCall(0U));

    // When call operator is called on the ProxyMethod handing the arg over as temporaries
    auto& proxy_method = *(this->unit_);
    auto call_result = proxy_method(42, 3.14, 'a');

    // Then no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TYPED_TEST(ProxyMethodWithInArgsTestFixture, CallOperator_PropagatesBindingError)
{
    this->GivenAValidProxyMethod();

    // Expecting that DoCall will be called on the binding which returns an error
    const auto binding_error_code = ComErrc::kBindingFailure;
    EXPECT_CALL(this->proxy_method_binding_mock_, DoCall(0U)).WillOnce(Return(MakeUnexpected(binding_error_code)));

    // When call operator is called on the ProxyMethod
    auto& proxy_method = *(this->unit_);
    auto call_result = proxy_method(42, 3.14, 'a');

    // Then the same error is returned
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TYPED_TEST(ProxyMethodWithoutInArgsTestFixture, CallOperator_NoArgs)
{
    this->GivenAValidProxyMethod();

    // When call operator is called on the ProxyMethod
    auto& proxy_method = *(this->unit_);
    auto call_result = proxy_method();

    // then no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TYPED_TEST(ProxyMethodWithoutInArgsTestFixture, CallOperator_PropagatesBindingError)
{
    this->GivenAValidProxyMethod();

    // Expecting that DoCall will be called on the binding which returns an error
    const auto binding_error_code = ComErrc::kBindingFailure;
    EXPECT_CALL(this->proxy_method_binding_mock_, DoCall(0U)).WillOnce(Return(MakeUnexpected(binding_error_code)));

    // When call operator is called on the ProxyMethod
    auto& proxy_method = *(this->unit_);
    auto call_result = proxy_method();

    // Then the same error is returned
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TYPED_TEST(ProxyMethodWithInArgsTestFixture, CallOperator_ZeroCopy)
{
    this->GivenAValidProxyMethod();

    // and given that Allocate was called on the ProxyMethod
    auto& proxy_method = *(this->unit_);
    auto method_in_arg_ptr_tuple = proxy_method.Allocate();

    // Expecting that DoCall will be called on the binding
    EXPECT_CALL(this->proxy_method_binding_mock_, DoCall(0U));

    // When filling the allocated argument storage and calling the call operator with the allocated argument pointers
    auto [method_in_arg_ptr_0, method_in_arg_ptr_1, method_in_arg_ptr_2] = std::move(method_in_arg_ptr_tuple.value());
    *method_in_arg_ptr_0 = 42;
    *method_in_arg_ptr_1 = 3.14;
    *method_in_arg_ptr_2 = 'a';
    auto call_result =
        proxy_method(std::move(method_in_arg_ptr_0), std::move(method_in_arg_ptr_1), std::move(method_in_arg_ptr_2));

    // Then no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TEST_F(ProxyMethodWithInArgsAndReturnFixture, CallOperator_ReturnsReturnTypePointerPointingToQueuePositionZeroOfBuffer)
{
    auto* const return_buffer_start_address = &(this->method_return_type_buffer_[0]);

    this->GivenAValidProxyMethod();

    // Expecting that AllocateReturnType will be called on the binding
    EXPECT_CALL(this->proxy_method_binding_mock_, AllocateReturnType(0U));

    // When call operator is called on the ProxyMethod
    auto& proxy_method = *(this->unit_);
    auto method_in_arg_ptr = proxy_method(kDummyArg1, kDummyArg2, kDummyArg3);

    // Then a valid MethodReturnTypePtr is returned
    ASSERT_TRUE(method_in_arg_ptr.has_value());

    // and the MethodReturnTypePtr points to queue position 0 of the buffer
    EXPECT_EQ(method_in_arg_ptr.value().GetQueuePosition(), 0);
    auto* const pointed_to_address = reinterpret_cast<std::byte*>(method_in_arg_ptr.value().get());
    EXPECT_EQ(pointed_to_address, return_buffer_start_address);
}

TEST_F(ProxyMethodWithReturnOnlyFixture, CallOperator_ReturnsReturnTypePointerPointingToQueuePositionZeroOfBuffer)
{
    auto* const return_buffer_start_address = &(this->method_return_type_buffer_[0]);

    this->GivenAValidProxyMethod();

    // Expecting that AllocateReturnType will be called on the binding
    EXPECT_CALL(this->proxy_method_binding_mock_, AllocateReturnType(0U));

    // When call operator is called on the ProxyMethod
    auto& proxy_method = *(this->unit_);
    auto method_in_arg_ptr = proxy_method();

    // Then a valid MethodReturnTypePtr is returned
    ASSERT_TRUE(method_in_arg_ptr.has_value());

    // and the MethodReturnTypePtr points to queue position 0 of the buffer
    EXPECT_EQ(method_in_arg_ptr.value().GetQueuePosition(), 0);
    auto* const pointed_to_address = reinterpret_cast<std::byte*>(method_in_arg_ptr.value().get());
    EXPECT_EQ(pointed_to_address, return_buffer_start_address);
}

TEST_F(ProxyMethodWithInArgsAndReturnFixture, ProxyMethodView_ReturnsTypeErasedInArgs)
{
    this->GivenAValidProxyMethod();

    // and given a ProxyMethodView created with the ProxyMethod
    ProxyMethodView proxy_method_view{*this->unit_};

    // When we call GetTypeErasedInArgs
    auto type_erased_in_args = proxy_method_view.GetTypeErasedInArgs();

    // Then we get a valid TypeErasedDataTypeInfo
    EXPECT_TRUE(type_erased_in_args.has_value());
}

TEST_F(ProxyMethodWithInArgsAndReturnFixture, ProxyMethodView_ReturnsTypeErasedReturnType)
{
    this->GivenAValidProxyMethod();

    // and given a ProxyMethodView created with the ProxyMethod
    ProxyMethodView proxy_method_view{*this->unit_};

    // When we call GetTypeErasedReturnType
    auto type_erased_return_type = proxy_method_view.GetTypeErasedReturnType();

    // Then we get a valid TypeErasedDataTypeInfo
    EXPECT_TRUE(type_erased_return_type.has_value());
}

TEST_F(ProxyMethodWithInArgsOnlyFixture, ProxyMethodView_ReturnsTypeErasedInArgs)
{
    this->GivenAValidProxyMethod();

    // and given a ProxyMethodView created with the ProxyMethod
    ProxyMethodView proxy_method_view{*this->unit_};

    // When we call GetTypeErasedInArgs
    auto type_erased_in_args = proxy_method_view.GetTypeErasedInArgs();

    // Then we get a valid TypeErasedDataTypeInfo
    EXPECT_TRUE(type_erased_in_args.has_value());
}

TEST_F(ProxyMethodWithInArgsOnlyFixture, ProxyMethodView_DoesNotReturnTypeErasedReturnType)
{
    this->GivenAValidProxyMethod();

    // and given a ProxyMethodView created with the ProxyMethod
    ProxyMethodView proxy_method_view{*this->unit_};

    // When we call GetTypeErasedReturnType
    auto type_erased_return_type = proxy_method_view.GetTypeErasedReturnType();

    // Then we don't get a valid TypeErasedDataTypeInfo
    EXPECT_FALSE(type_erased_return_type.has_value());
}

using ProxyMethodWithReturnOnlyFixture = ProxyMethodTestFixture<ProxyMethodWithReturnOnly>;
TEST_F(ProxyMethodWithReturnOnlyFixture, ProxyMethodView_DoesNotReturnTypeErasedInArgs)
{
    this->GivenAValidProxyMethod();

    // and given a ProxyMethodView created with the ProxyMethod
    ProxyMethodView proxy_method_view{*this->unit_};

    // When we call GetTypeErasedInArgs
    auto type_erased_in_args = proxy_method_view.GetTypeErasedInArgs();

    // Then we don't get a valid TypeErasedDataTypeInfo
    EXPECT_FALSE(type_erased_in_args.has_value());
}

TEST_F(ProxyMethodWithReturnOnlyFixture, ProxyMethodView_ReturnsTypeErasedReturnType)
{
    this->GivenAValidProxyMethod();

    // and given a ProxyMethodView created with the ProxyMethod
    ProxyMethodView proxy_method_view{*this->unit_};

    // When we call GetTypeErasedReturnType
    auto type_erased_return_type = proxy_method_view.GetTypeErasedReturnType();

    // Then we get a valid TypeErasedDataTypeInfo
    EXPECT_TRUE(type_erased_return_type.has_value());
}

using ProxyMethodWithNoInArgsOrReturnFixture = ProxyMethodTestFixture<ProxyMethodWithNoInArgsOrReturn>;
TEST_F(ProxyMethodWithNoInArgsOrReturnFixture, ProxyMethodView_DoesNotReturnTypeErasedInArgs)
{
    this->GivenAValidProxyMethod();

    // and given a ProxyMethodView created with the ProxyMethod
    ProxyMethodView proxy_method_view{*this->unit_};

    // When we call GetTypeErasedInArgs
    auto type_erased_in_args = proxy_method_view.GetTypeErasedInArgs();

    // Then we don't get a valid TypeErasedDataTypeInfo
    EXPECT_FALSE(type_erased_in_args.has_value());
}

TEST_F(ProxyMethodWithNoInArgsOrReturnFixture, ProxyMethodView_DoesNotReturnTypeErasedReturnType)
{
    this->GivenAValidProxyMethod();

    // and given a ProxyMethodView created with the ProxyMethod
    ProxyMethodView proxy_method_view{*this->unit_};

    // When we call GetTypeErasedReturnType
    auto type_erased_return_type = proxy_method_view.GetTypeErasedReturnType();

    // Then we don't get a valid TypeErasedDataTypeInfo
    EXPECT_FALSE(type_erased_return_type.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl
