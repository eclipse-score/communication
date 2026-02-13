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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_METHOD_CALL_REGISTRATION_GUARD_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_METHOD_CALL_REGISTRATION_GUARD_H

#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include "score/language/safecpp/scoped_function/move_only_scoped_function.h"
#include "score/language/safecpp/scoped_function/scope.h"
#include "score/utils/src/scoped_operation.h"

namespace score::mw::com::impl::lola
{

class IMessagePassingService;

using MethodCallRegistrationGuard = std::unique_ptr<utils::ScopedOperation<safecpp::MoveOnlyScopedFunction<void()>>>;

/// \brief RAII class which will call UnregisterMethodCallHandler on destruction.
///
/// Will be returned by MessagePassingService::RegisterMethodCallHandler() to allow a user to unregister the registered
/// handler.
class MethodCallRegistrationGuardFactory
{
  public:
    static MethodCallRegistrationGuard Create(IMessagePassingService& message_passing_service,
                                              const QualityType asil_level,
                                              const ProxyMethodInstanceIdentifier proxy_method_instance_identifier,
                                              const safecpp::Scope<>& message_passing_service_instance_scope);
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_METHOD_CALL_REGISTRATION_GUARD_H
