/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/methods/generic_proxy_method.h"

#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_method.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/proxy_base.h"

#include "score/result/result.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

const std::string_view kMethodName{"DummyGenericMethod"};
// The binding still takes a queue position; GenericProxyMethod always hands it slot 0
// while the call queue is fixed at size 1. Tests assert that it's slot 0 that flows through.
constexpr std::size_t kOnlyQueuePosition{0U};

class TestProxyBase : public ProxyBase
{
  public:
    using ProxyBase::ProxyBase;
    const ProxyBase::ProxyMethods& GetMethods()
    {
        return methods_;
    }
};

class GenericProxyMethodFixture : public ::testing::Test
{
  public:
    GenericProxyMethodFixture() : proxy_base_{std::make_unique<mock_binding::Proxy>(), config_store_.GetHandle()} {}

    std::unique_ptr<GenericProxyMethod> MakeUnit()
    {
        return std::make_unique<GenericProxyMethod>(
            proxy_base_, std::make_unique<mock_binding::ProxyMethodFacade>(proxy_method_binding_mock_), kMethodName);
    }

    alignas(8) std::array<std::byte, 64> in_args_buffer_{};
    alignas(8) std::array<std::byte, 64> return_buffer_{};

    ConfigurationStore config_store_{InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value(),
                                     make_ServiceIdentifierType("foo"),
                                     QualityType::kASIL_QM,
                                     LolaServiceTypeDeployment{42U},
                                     LolaServiceInstanceDeployment{1U}};
    mock_binding::ProxyMethod proxy_method_binding_mock_{};
    TestProxyBase proxy_base_;
};

TEST_F(GenericProxyMethodFixture, RegistersItselfWithParentProxyOnConstruction)
{
    // Given a parent proxy with no registered methods
    ASSERT_TRUE(proxy_base_.GetMethods().empty());

    // When a GenericProxyMethod is constructed
    const auto unit = MakeUnit();

    // Then it is reachable via the parent's method map under its name
    const auto& methods = proxy_base_.GetMethods();
    ASSERT_EQ(methods.size(), 1U);
    const auto it = methods.find(kMethodName);
    ASSERT_NE(it, methods.end());
    EXPECT_EQ(&it->second.get(), unit.get());
}

TEST_F(GenericProxyMethodFixture, AllocateInArgsDelegatesToBinding)
{
    score::cpp::span<std::byte> expected{in_args_buffer_.data(), in_args_buffer_.size()};
    EXPECT_CALL(proxy_method_binding_mock_, GetInArgsBuffer(kOnlyQueuePosition))
        .WillOnce(Return(score::Result<score::cpp::span<std::byte>>{expected}));

    auto unit = MakeUnit();

    const auto result = unit->AllocateInArgs();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().data(), expected.data());
    EXPECT_EQ(result.value().size(), expected.size());
}

TEST_F(GenericProxyMethodFixture, AllocateReturnTypeDelegatesToBinding)
{
    score::cpp::span<std::byte> expected{return_buffer_.data(), return_buffer_.size()};
    EXPECT_CALL(proxy_method_binding_mock_, GetReturnValueBuffer(kOnlyQueuePosition))
        .WillOnce(Return(score::Result<score::cpp::span<std::byte>>{expected}));

    auto unit = MakeUnit();

    const auto result = unit->AllocateReturnType();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().data(), expected.data());
    EXPECT_EQ(result.value().size(), expected.size());
}

TEST_F(GenericProxyMethodFixture, DoCallDelegatesToBinding)
{
    EXPECT_CALL(proxy_method_binding_mock_, DoCall(kOnlyQueuePosition)).WillOnce(Return(ResultBlank{}));

    auto unit = MakeUnit();

    const auto result = unit->DoCall();
    EXPECT_TRUE(result.has_value());
}

TEST_F(GenericProxyMethodFixture, DoCallPropagatesBindingError)
{
    EXPECT_CALL(proxy_method_binding_mock_, DoCall(kOnlyQueuePosition))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    auto unit = MakeUnit();

    const auto result = unit->DoCall();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericProxyMethodFixture, InvalidBindingInConstructorMarksServiceElementAsInvalid)
{
    // When a GenericProxyMethod is created with an invalid binding
    GenericProxyMethod unit{proxy_base_, nullptr, kMethodName};

    // Then calling AreBindingsValid returns false
    EXPECT_FALSE(ProxyBaseView{proxy_base_}.AreBindingsValid());
}

TEST_F(GenericProxyMethodFixture, MoveConstructingUpdatesRegistrationInParentProxy)
{
    auto original = MakeUnit();
    const auto* const original_address = original.get();

    // Sanity check: the parent currently points at the original.
    {
        const auto& methods = proxy_base_.GetMethods();
        const auto it = methods.find(kMethodName);
        ASSERT_NE(it, methods.end());
        ASSERT_EQ(&it->second.get(), original_address);
    }

    // When move-constructing a new GenericProxyMethod from the original
    GenericProxyMethod moved_to{std::move(*original)};

    // Then the parent's method map now points at the moved-to object
    const auto& methods = proxy_base_.GetMethods();
    const auto it = methods.find(kMethodName);
    ASSERT_NE(it, methods.end());
    EXPECT_EQ(&it->second.get(), &moved_to);
    EXPECT_NE(&it->second.get(), original_address);
}

}  // namespace
}  // namespace score::mw::com::impl
