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

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"

#include "score/language/safecpp/scoped_function/move_only_scoped_function.h"
#include "score/mw/log/logging.h"

namespace score::mw::com::impl::lola
{

MethodSubscriptionRegistrationGuard MethodSubscriptionRegistrationGuardFactory::Create(
    IMessagePassingService& message_passing_service,
    const QualityType asil_level,
    const SkeletonInstanceIdentifier skeleton_instance_identifier,
    const safecpp::Scope<>& message_passing_service_instance_scope)
{
    return std::make_unique<utils::ScopedOperation<safecpp::MoveOnlyScopedFunction<void()>>>(
        safecpp::MoveOnlyScopedFunction<void()>{message_passing_service_instance_scope,
                                                [&message_passing_service, asil_level, skeleton_instance_identifier]() {
                                                    message_passing_service.UnregisterOnServiceMethodSubscribedHandler(
                                                        asil_level, skeleton_instance_identifier);
                                                }});
}

}  // namespace score::mw::com::impl::lola
