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
///
/// @file
/// @copyright Copyright (C) 2023, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
///

#ifndef SCORE_MW_COM_IMPL_SKELETON_TRACING_H
#define SCORE_MW_COM_IMPL_SKELETON_TRACING_H

#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/skeleton_binding.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/impl/skeleton_field_base.h"

#include <map>
#include <optional>

namespace score::mw::com::impl::tracing
{

std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> CreateRegisterShmObjectCallback(
    const InstanceIdentifier& instance_id,
    const std::map<std::string_view, std::reference_wrapper<SkeletonEventBase>>& events,
    const std::map<std::string_view, std::reference_wrapper<SkeletonFieldBase>>& fields,
    const SkeletonBinding& skeleton_binding) noexcept;
std::optional<SkeletonBinding::UnregisterShmObjectTraceCallback> CreateUnregisterShmObjectCallback(
    const InstanceIdentifier& instance_id,
    const std::map<std::string_view, std::reference_wrapper<SkeletonEventBase>>& events,
    const std::map<std::string_view, std::reference_wrapper<SkeletonFieldBase>>& fields,
    const SkeletonBinding& skeleton_binding) noexcept;

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_SKELETON_TRACING_H
