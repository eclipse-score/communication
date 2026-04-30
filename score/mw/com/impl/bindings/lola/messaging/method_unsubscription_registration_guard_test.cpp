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

#include "score/mw/com/impl/bindings/lola/messaging/method_unsubscription_registration_guard.h"

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_mock.h"
#include "score/mw/com/impl/bindings/lola/skeleton_instance_identifier.h"
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

class MethodUnsubscriptionRegistrationGuardFixture : public ::testing::Test
{
  public:
    MethodUnsubscriptionRegistrationGuardFixture()
    {
        ON_CALL(message_passing_service_mock_, UnregisterOnServiceMethodUnsubscribedHandler(_, _))
            .WillByDefault(WithoutArgs(Invoke([this]() -> void {
                unregister_on_service_method_unsubscribed_handler_called_ = true;
            })));
    }

    MethodUnsubscriptionRegistrationGuardFixture& GivenAMethodUnsubscriptionRegistrationGuard()
    {
        score::cpp::ignore =
            method_unsubscription_registration_guard_.emplace(MethodUnsubscriptionRegistrationGuardFactory::Create(
                message_passing_service_mock_, kAsilLevel, kSkeletonInstanceIdentifier, scope_));
        return *this;
    }

    MessagePassingServiceMock message_passing_service_mock_{};
    std::optional<MethodUnsubscriptionRegistrationGuard> method_unsubscription_registration_guard_{};
    bool unregister_on_service_method_unsubscribed_handler_called_{false};

    safecpp::Scope<> scope_{};
};

TEST_F(MethodUnsubscriptionRegistrationGuardFixture, MethodUnsubscriptionRegistrationGuardUsesScopeExit)
{
    // Expecting that MethodUnsubscriptionRegistrationGuard is a type alias for utils::ScopeExit. If this is
    // the case, then we only add basic tests here that UnregisterOnServiceMethodUnsubscribedHandler is called on
    // destruction of the guard and that the scope of the guard is correctly handled. The more complex tests about
    // testing whether the handler is called when move constructing / move assigning the guard is handled in the tests
    // for ScopeExit.
    static_assert(std::is_same_v<MethodUnsubscriptionRegistrationGuard,
                                 utils::ScopeExit<safecpp::MoveOnlyScopedFunction<void()>>>);
}

TEST_F(MethodUnsubscriptionRegistrationGuardFixture, CreatingGuardDoesNotCallUnregister)
{
    // When creating a MethodUnsubscriptionRegistrationGuard
    GivenAMethodUnsubscriptionRegistrationGuard();

    // Expecting that UnregisterOnServiceMethodUnsubscribedHandler is never called
    EXPECT_CALL(message_passing_service_mock_, UnregisterOnServiceMethodUnsubscribedHandler(_, _)).Times(0);

    // Then UnregisterOnServiceMethodUnsubscribedHandler is not called
    EXPECT_FALSE(unregister_on_service_method_unsubscribed_handler_called_);
}

TEST_F(MethodUnsubscriptionRegistrationGuardFixture, DestroyingGuardCallsUnregister)
{
    GivenAMethodUnsubscriptionRegistrationGuard();

    // Expecting that UnregisterOnServiceMethodUnsubscribedHandler is called with the asil_level and
    // SkeletonInstanceIdentifier used to create the guard
    EXPECT_CALL(message_passing_service_mock_,
                UnregisterOnServiceMethodUnsubscribedHandler(kAsilLevel, kSkeletonInstanceIdentifier));

    // When destroying the MethodUnsubscriptionRegistrationGuard
    method_unsubscription_registration_guard_.reset();

    // Then UnregisterOnServiceMethodUnsubscribedHandler is called
    EXPECT_TRUE(unregister_on_service_method_unsubscribed_handler_called_);
}

TEST_F(MethodUnsubscriptionRegistrationGuardFixture, DestroyingGuardAfterScopeHasExpiredDoesNotCallUnregister)
{
    GivenAMethodUnsubscriptionRegistrationGuard();

    // and given that the scope has expired
    scope_.Expire();

    // Expecting that UnregisterOnServiceMethodUnsubscribedHandler is never called
    EXPECT_CALL(message_passing_service_mock_, UnregisterOnServiceMethodUnsubscribedHandler(_, _)).Times(0);

    // When destroying the MethodUnsubscriptionRegistrationGuard
    method_unsubscription_registration_guard_.reset();

    // Then UnregisterOnServiceMethodUnsubscribedHandler is not called
    EXPECT_FALSE(unregister_on_service_method_unsubscribed_handler_called_);
}

}  // namespace
}  // namespace score::mw::com::impl::lola::detail
