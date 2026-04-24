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

#include "score/mw/com/impl/bindings/lola/messaging/method_call_registration_guard.h"

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_mock.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola::detail
{
namespace
{

using namespace ::testing;

const QualityType kAsilLevel{QualityType::kASIL_B};
const ProxyMethodInstanceIdentifier kProxyMethodInstanceIdentifier{{5U, 1U}, {LolaMethodId{55U}}};

class MethodCallRegistrationGuardFixture : public ::testing::Test
{
  public:
    MethodCallRegistrationGuardFixture()
    {
        ON_CALL(message_passing_service_mock_, UnregisterMethodCallHandler(_, _))
            .WillByDefault(WithoutArgs(Invoke([this]() -> Result<void> {
                unregister_method_call_handler_called_ = true;
                return {};
            })));
    }

    MethodCallRegistrationGuardFixture& GivenAMethodCallRegistrationGuard()
    {
        score::cpp::ignore = method_call_registration_guard_.emplace(MethodCallRegistrationGuardFactory::Create(
            message_passing_service_mock_, kAsilLevel, kProxyMethodInstanceIdentifier, scope_));
        return *this;
    }

    MessagePassingServiceMock message_passing_service_mock_{};
    std::optional<MethodCallRegistrationGuard> method_call_registration_guard_{};
    bool unregister_method_call_handler_called_{false};

    safecpp::Scope<> scope_{};
};

TEST_F(MethodCallRegistrationGuardFixture, MethodCallRegistrationGuardUsesScopeExit)
{
    // Expecting that MethodCallRegistrationGuard is a type alias for utils::ScopeExit. If this is the
    // case, then we only add basic tests here that UnregisterMethodCallHandler is called on destruction of the guard
    // and that the scope of the guard is correctly handled. The more complex tests about testing whether the handler is
    // called when move constructing / move assigning the guard is handled in the tests for ScopeExit.
    static_assert(
        std::is_same_v<MethodCallRegistrationGuard, utils::ScopeExit<safecpp::MoveOnlyScopedFunction<void()>>>);
}

TEST_F(MethodCallRegistrationGuardFixture, CreatingGuardDoesNotCallUnregister)
{
    // Expecting that UnregisterMethodCallHandler is never called
    EXPECT_CALL(message_passing_service_mock_, UnregisterMethodCallHandler(_, _)).Times(0);

    // When creating a MethodCallRegistrationGuard
    GivenAMethodCallRegistrationGuard();

    // Then UnregisterMethodCallHandler is not called
    EXPECT_FALSE(unregister_method_call_handler_called_);
}

TEST_F(MethodCallRegistrationGuardFixture, DestroyingGuardCallsUnregister)
{
    GivenAMethodCallRegistrationGuard();

    // Expecting that UnregisterMethodCallHandler is called with the asil_level and
    // ProxyMethodInstanceIdentifier used to create the guard
    EXPECT_CALL(message_passing_service_mock_, UnregisterMethodCallHandler(kAsilLevel, kProxyMethodInstanceIdentifier));

    // When destroying the MethodCallRegistrationGuard
    method_call_registration_guard_.reset();

    // Then UnregisterMethodCallHandler is called
    EXPECT_TRUE(unregister_method_call_handler_called_);
}

TEST_F(MethodCallRegistrationGuardFixture, DestroyingGuardAfterScopeHasExpiredDoesNotCallUnregister)
{
    GivenAMethodCallRegistrationGuard();

    // and given that the scope has expired
    scope_.Expire();

    // Expecting that UnregisterMethodCallHandler is never called
    EXPECT_CALL(message_passing_service_mock_, UnregisterMethodCallHandler(_, _)).Times(0);

    // When destroying the MethodCallRegistrationGuard
    method_call_registration_guard_.reset();

    // Then UnregisterMethodCallHandler is not called
    EXPECT_FALSE(unregister_method_call_handler_called_);
}

}  // namespace
}  // namespace score::mw::com::impl::lola::detail
