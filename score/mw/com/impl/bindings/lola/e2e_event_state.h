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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_E2E_EVENT_STATE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_E2E_EVENT_STATE_H

#include "score/mw/com/impl/configuration/e2e_event_type_deployment.h"
#include "score/mw/com/impl/e2e/e2e_profile_stub.h"

#include <memory>
#include <optional>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief E2E state of one LoLa event binding. Context is e2e::ProtectContext on the provider side and
///        e2e::CheckContext on the consumer side.
template <typename Context>
struct E2EEventState
{
    std::shared_ptr<e2e::HeaderStorage> header_storage;
    e2e::ProfileConfiguration profile_configuration;
    Context context;
};

/// \brief Creates the E2E state of an event; resolved once at construction.
/// \return Empty unless the event has a deployment and a header storage, so a state always has a non-null
///         header_storage.
template <typename Context>
std::optional<E2EEventState<Context>> MakeE2EEventState(
    const std::optional<E2EEventTypeDeployment>& e2e_event_deployment,
    std::shared_ptr<e2e::HeaderStorage> e2e_header_storage) noexcept
{
    if (!e2e_event_deployment.has_value() || (e2e_header_storage == nullptr))
    {
        return std::nullopt;
    }
    return E2EEventState<Context>{
        std::move(e2e_header_storage),
        e2e::ProfileConfiguration{e2e_event_deployment->data_id_, e2e_event_deployment->max_delta_counter_},
        Context{}};
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_E2E_EVENT_STATE_H
