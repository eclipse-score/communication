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
#include "score/mw/com/impl/bindings/someip/e2e_event_protection.h"

#include <string>

namespace score::mw::com::impl::someip
{
namespace
{

const E2EEventTypeDeployment* FindE2EEventDeployment(const SomeIpServiceTypeDeployment& type_deployment,
                                                     const std::string_view event_name)
{
    const auto deployment_it = type_deployment.e2e_events_.find(std::string{event_name});
    if (deployment_it == type_deployment.e2e_events_.cend())
    {
        return nullptr;
    }
    return &deployment_it->second;
}

}  // namespace

E2EEventProtector::E2EEventProtector(const E2EEventTypeDeployment& deployment) noexcept
    : configuration_{deployment.data_id_, deployment.max_delta_counter_}, context_{}
{
}

bool E2EEventProtector::Protect(const score::cpp::span<std::byte> frame) noexcept
{
    if (frame.size() < kE2EHeaderSize)
    {
        return false;
    }
    return e2e::ProtectMessage(frame.first(kE2EHeaderSize), frame.subspan(kE2EHeaderSize), configuration_, context_);
}

E2EEventChecker::E2EEventChecker(const E2EEventTypeDeployment& deployment) noexcept
    : configuration_{deployment.data_id_, deployment.max_delta_counter_}, context_{}
{
}

e2e::RawCheckOutcome E2EEventChecker::Check(const score::cpp::span<const std::byte> frame) noexcept
{
    if (frame.size() < kE2EHeaderSize)
    {
        return {e2e::DataIntegrityStatus::kError, e2e::SequenceStatus::kSkipped};
    }
    return e2e::CheckMessage(frame.first(kE2EHeaderSize), frame.subspan(kE2EHeaderSize), configuration_, context_);
}

std::optional<E2EEventProtector> CreateE2EEventProtector(const SomeIpServiceTypeDeployment& type_deployment,
                                                         const std::string_view event_name)
{
    const auto* const deployment = FindE2EEventDeployment(type_deployment, event_name);
    if (deployment == nullptr)
    {
        return std::nullopt;
    }
    return E2EEventProtector{*deployment};
}

std::optional<E2EEventChecker> CreateE2EEventChecker(const SomeIpServiceTypeDeployment& type_deployment,
                                                     const std::string_view event_name)
{
    const auto* const deployment = FindE2EEventDeployment(type_deployment, event_name);
    if (deployment == nullptr)
    {
        return std::nullopt;
    }
    return E2EEventChecker{*deployment};
}

}  // namespace score::mw::com::impl::someip
