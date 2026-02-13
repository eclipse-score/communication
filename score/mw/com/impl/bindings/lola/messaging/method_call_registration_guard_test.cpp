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
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include <gtest/gtest.h>

namespace score::mw::com::impl::lola::detail
{
namespace
{

using namespace ::testing;

const QualityType kAsilLevel{QualityType::kASIL_B};
const ProxyMethodInstanceIdentifier kProxyMethodInstanceIdentifier{{5U, 1U}, LolaMethodId{55U}};

class RegistrationGuardFixture : public ::testing::Test
{
  public:
    RegistrationGuardFixture()
    {
        ON_CALL(message_passing_service_mock_, UnregisterMethodCallHandler(_, _))
            .WillByDefault(WithoutArgs(Invoke([this]() -> ResultBlank {
                unregister_method_call_handler_called_ = true;
                return {};
            })));

        ON_CALL(message_passing_service_mock_2_, UnregisterMethodCallHandler(_, _))
            .WillByDefault(WithoutArgs(Invoke([this]() -> ResultBlank {
                unregister_method_call_handler_called_2_ = true;
                return {};
            })));
    }

    RegistrationGuardFixture& GivenAMethodCallRegistrationGuard()
    {
        score::cpp::ignore = method_call_registration_guard_.emplace(MethodCallRegistrationGuardFactory::Create(
            message_passing_service_mock_, kAsilLevel, kProxyMethodInstanceIdentifier, scope_));
        return *this;
    }

    RegistrationGuardFixture& GivenTwoMethodCallRegistrationGuards()
    {
        score::cpp::ignore = method_call_registration_guard_.emplace(MethodCallRegistrationGuardFactory::Create(
            message_passing_service_mock_, kAsilLevel, kProxyMethodInstanceIdentifier, scope_));
        score::cpp::ignore = method_call_registration_guard_2_.emplace(MethodCallRegistrationGuardFactory::Create(
            message_passing_service_mock_2_, kAsilLevel, kProxyMethodInstanceIdentifier, scope_));
        return *this;
    }

    MessagePassingServiceMock message_passing_service_mock_{};
    std::optional<MethodCallRegistrationGuard> method_call_registration_guard_{};
    bool unregister_method_call_handler_called_{false};

    MessagePassingServiceMock message_passing_service_mock_2_{};
    std::optional<MethodCallRegistrationGuard> method_call_registration_guard_2_{};
    bool unregister_method_call_handler_called_2_{false};

    safecpp::Scope<> scope_{};
};

using MethodCallRegistrationGuardFixture = RegistrationGuardFixture;
TEST_F(MethodCallRegistrationGuardFixture, CreatingGuardDoesNotCallUnregister)
{
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

    // When destroying the MethodCallRegistrationGuard
    method_call_registration_guard_.reset();

    // Then UnregisterMethodCallHandler is not called
    EXPECT_FALSE(unregister_method_call_handler_called_);
}

TEST_F(MethodCallRegistrationGuardFixture, MoveConstructingGuardDoesNotCallUnregister)
{
    GivenAMethodCallRegistrationGuard();

    // When move constructing a new MethodCallRegistrationGuard
    MethodCallRegistrationGuard moved_to_guard{std::move(method_call_registration_guard_).value()};

    // Then UnregisterMethodCallHandler is not called
    EXPECT_FALSE(unregister_method_call_handler_called_);
}

TEST_F(MethodCallRegistrationGuardFixture, DestroyingMoveConstructedMovedFromGuardDoesNotCallUnregister)
{
    GivenAMethodCallRegistrationGuard();

    // and given a new MethodCallRegistrationGuard move constructed from another
    MethodCallRegistrationGuard moved_to_guard{std::move(method_call_registration_guard_).value()};

    // When destroying the moved_from guard
    method_call_registration_guard_.reset();

    // Then UnregisterMethodCallHandler is not called
    EXPECT_FALSE(unregister_method_call_handler_called_);
}

TEST_F(MethodCallRegistrationGuardFixture, DestroyingMoveConstructedMovedToGuardCallsUnregister)
{
    GivenAMethodCallRegistrationGuard();

    // and given a new MethodCallRegistrationGuard move constructed from another
    MethodCallRegistrationGuard moved_to_guard{std::move(method_call_registration_guard_).value()};

    // When destroying the moved_to guard
    moved_to_guard.reset();

    // Then UnregisterMethodCallHandler is called
    EXPECT_TRUE(unregister_method_call_handler_called_);
}

TEST_F(MethodCallRegistrationGuardFixture, MoveAssigningGuardCallsUnregisterOnMovedToGuard)
{
    GivenTwoMethodCallRegistrationGuards();

    // When move assigning one MethodCallRegistrationGuard to another
    method_call_registration_guard_.value() = std::move(method_call_registration_guard_2_).value();

    // Then UnregisterMethodCallHandler is only called on the moved-to guard
    EXPECT_TRUE(unregister_method_call_handler_called_);
    EXPECT_FALSE(unregister_method_call_handler_called_2_);
}

TEST_F(MethodCallRegistrationGuardFixture, DestroyingMoveAssignedMovedFromGuardDoesNotCallUnregister)
{
    GivenTwoMethodCallRegistrationGuards();

    // and given that one MethodCallRegistrationGuard to was move assigned to another
    method_call_registration_guard_.value() = std::move(method_call_registration_guard_2_).value();

    // When destroying the moved-from guard
    method_call_registration_guard_2_.reset();

    // Then UnregisterMethodCallHandler is not called on the moved-from guard
    EXPECT_FALSE(unregister_method_call_handler_called_2_);
}

TEST_F(MethodCallRegistrationGuardFixture, DestroyingMoveAssignedMovedToGuardCallsUnregister)
{
    GivenTwoMethodCallRegistrationGuards();

    // and given that one MethodCallRegistrationGuard to was move assigned to another
    method_call_registration_guard_.value() = std::move(method_call_registration_guard_2_).value();

    // When destroying the moved-to guard
    method_call_registration_guard_.reset();

    // Then UnregisterMethodCallHandler is not called on the moved-to guard
    EXPECT_TRUE(unregister_method_call_handler_called_2_);
}

}  // namespace
}  // namespace score::mw::com::impl::lola::detail
