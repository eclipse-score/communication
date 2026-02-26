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

#include "score/mw/com/impl/bindings/lola/messaging/method_subscription_registration_guard.h"

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_mock.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include <gtest/gtest.h>

namespace score::mw::com::impl::lola::detail
{
namespace
{

using namespace ::testing;

const QualityType kAsilLevel{QualityType::kASIL_B};
const SkeletonInstanceIdentifier kSkeletonInstanceIdentifier{LolaServiceId{12U},
                                                             LolaServiceInstanceId::InstanceId{22U}};

class MethodSubscriptionRegistrationGuardFixture : public ::testing::Test
{
  public:
    MethodSubscriptionRegistrationGuardFixture()
    {
        ON_CALL(message_passing_service_mock_, UnregisterOnServiceMethodSubscribedHandler(_, _))
            .WillByDefault(WithoutArgs(Invoke([this]() -> ResultBlank {
                unregister_on_service_method_subscribed_handler_called_ = true;
                return {};
            })));

        ON_CALL(message_passing_service_mock_2_, UnregisterOnServiceMethodSubscribedHandler(_, _))
            .WillByDefault(WithoutArgs(Invoke([this]() -> ResultBlank {
                unregister_on_service_method_subscribed_handler_called_2_ = true;
                return {};
            })));
    }

    MethodSubscriptionRegistrationGuardFixture& GivenAMethodSubscriptionRegistrationGuard()
    {
        score::cpp::ignore =
            method_subscription_registration_guard_.emplace(MethodSubscriptionRegistrationGuardFactory::Create(
                message_passing_service_mock_, kAsilLevel, kSkeletonInstanceIdentifier, scope_));
        return *this;
    }

    MethodSubscriptionRegistrationGuardFixture& GivenTwoMethodSubscriptionRegistrationGuards()
    {
        score::cpp::ignore =
            method_subscription_registration_guard_.emplace(MethodSubscriptionRegistrationGuardFactory::Create(
                message_passing_service_mock_, kAsilLevel, kSkeletonInstanceIdentifier, scope_));
        score::cpp::ignore =
            method_subscription_registration_guard_2_.emplace(MethodSubscriptionRegistrationGuardFactory::Create(
                message_passing_service_mock_2_, kAsilLevel, kSkeletonInstanceIdentifier, scope_));
        return *this;
    }

    MessagePassingServiceMock message_passing_service_mock_{};
    std::optional<MethodSubscriptionRegistrationGuard> method_subscription_registration_guard_{};
    bool unregister_on_service_method_subscribed_handler_called_{false};

    MessagePassingServiceMock message_passing_service_mock_2_{};
    std::optional<MethodSubscriptionRegistrationGuard> method_subscription_registration_guard_2_{};
    bool unregister_on_service_method_subscribed_handler_called_2_{false};

    safecpp::Scope<> scope_{};
};

TEST_F(MethodSubscriptionRegistrationGuardFixture, CreatingGuardDoesNotCallUnregister)
{
    // When creating a MethodSubscriptionRegistrationGuard
    GivenAMethodSubscriptionRegistrationGuard();

    // Then UnregisterOnServiceMethodSubscribedHandler is not called
    EXPECT_FALSE(unregister_on_service_method_subscribed_handler_called_);
}

TEST_F(MethodSubscriptionRegistrationGuardFixture, DestroyingGuardCallsUnregister)
{
    GivenAMethodSubscriptionRegistrationGuard();

    // Expecting that UnregisterOnServiceMethodSubscribedHandler is called with the asil_level and
    // SkeletonInstanceIdentifier used to create the guard
    EXPECT_CALL(message_passing_service_mock_,
                UnregisterOnServiceMethodSubscribedHandler(kAsilLevel, kSkeletonInstanceIdentifier));

    // When destroying the MethodSubscriptionRegistrationGuard
    method_subscription_registration_guard_.reset();

    // Then UnregisterOnServiceMethodSubscribedHandler is called
    EXPECT_TRUE(unregister_on_service_method_subscribed_handler_called_);
}

TEST_F(MethodSubscriptionRegistrationGuardFixture, DestroyingGuardAfterScopeHasExpiredDoesNotCallUnregister)
{
    GivenAMethodSubscriptionRegistrationGuard();

    // and given that the scope has expired
    scope_.Expire();

    // When destroying the MethodSubscriptionRegistrationGuard
    method_subscription_registration_guard_.reset();

    // Then UnregisterOnServiceMethodSubscribedHandler is not called
    EXPECT_FALSE(unregister_on_service_method_subscribed_handler_called_);
}

TEST_F(MethodSubscriptionRegistrationGuardFixture, MoveConstructingGuardDoesNotCallUnregister)
{
    GivenAMethodSubscriptionRegistrationGuard();

    // When move constructing a new MethodSubscriptionRegistrationGuard
    MethodSubscriptionRegistrationGuard moved_to_guard{std::move(method_subscription_registration_guard_).value()};

    // Then UnregisterOnServiceMethodSubscribedHandler is not called
    EXPECT_FALSE(unregister_on_service_method_subscribed_handler_called_);
}

TEST_F(MethodSubscriptionRegistrationGuardFixture, DestroyingMoveConstructedMovedFromGuardDoesNotCallUnregister)
{
    GivenAMethodSubscriptionRegistrationGuard();

    // and given a new MethodSubscriptionRegistrationGuard move constructed from another
    MethodSubscriptionRegistrationGuard moved_to_guard{std::move(method_subscription_registration_guard_).value()};

    // When destroying the moved_from guard
    method_subscription_registration_guard_.reset();

    // Then UnregisterOnServiceMethodSubscribedHandler is not called
    EXPECT_FALSE(unregister_on_service_method_subscribed_handler_called_);
}

TEST_F(MethodSubscriptionRegistrationGuardFixture, DestroyingMoveConstructedMovedToGuardCallsUnregister)
{
    GivenAMethodSubscriptionRegistrationGuard();

    // and given a new MethodSubscriptionRegistrationGuard move constructed from another
    MethodSubscriptionRegistrationGuard moved_to_guard{std::move(method_subscription_registration_guard_).value()};

    // When destroying the moved_to guard
    moved_to_guard.reset();

    // Then UnregisterOnServiceMethodSubscribedHandler is called
    EXPECT_TRUE(unregister_on_service_method_subscribed_handler_called_);
}

TEST_F(MethodSubscriptionRegistrationGuardFixture, MoveAssigningGuardCallsUnregisterOnMovedToGuard)
{
    GivenTwoMethodSubscriptionRegistrationGuards();

    // When move assigning one MethodSubscriptionRegistrationGuard to another
    method_subscription_registration_guard_.value() = std::move(method_subscription_registration_guard_2_).value();

    // Then UnregisterOnServiceMethodSubscribedHandler is only called on the moved-to guard
    EXPECT_TRUE(unregister_on_service_method_subscribed_handler_called_);
    EXPECT_FALSE(unregister_on_service_method_subscribed_handler_called_2_);
}

TEST_F(MethodSubscriptionRegistrationGuardFixture, DestroyingMoveAssignedMovedFromGuardDoesNotCallUnregister)
{
    GivenTwoMethodSubscriptionRegistrationGuards();

    // and given that one MethodSubscriptionRegistrationGuard to was move assigned to another
    method_subscription_registration_guard_.value() = std::move(method_subscription_registration_guard_2_).value();

    // When destroying the moved-from guard
    method_subscription_registration_guard_2_.reset();

    // Then UnregisterOnServiceMethodSubscribedHandler is not called on the moved-from guard
    EXPECT_FALSE(unregister_on_service_method_subscribed_handler_called_2_);
}

TEST_F(MethodSubscriptionRegistrationGuardFixture, DestroyingMoveAssignedMovedToGuardCallsUnregister)
{
    GivenTwoMethodSubscriptionRegistrationGuards();

    // and given that one MethodSubscriptionRegistrationGuard to was move assigned to another
    method_subscription_registration_guard_.value() = std::move(method_subscription_registration_guard_2_).value();

    // When destroying the moved-to guard
    method_subscription_registration_guard_.reset();

    // Then UnregisterOnServiceMethodSubscribedHandler is not called on the moved-to guard
    EXPECT_TRUE(unregister_on_service_method_subscribed_handler_called_2_);
}

}  // namespace
}  // namespace score::mw::com::impl::lola::detail
