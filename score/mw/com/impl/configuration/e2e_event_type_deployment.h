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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_E2E_EVENT_TYPE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_E2E_EVENT_TYPE_DEPLOYMENT_H

#include <cstdint>

namespace score::mw::com::impl
{

/// \brief E2E profile selected for the POC event deployment.
enum class E2EProfile : std::uint8_t
{
    kP4 = 1U,
};

/// \brief Binding-specific POC E2E parameters shared by event providers and consumers.
struct E2EEventTypeDeployment
{
    E2EProfile profile_;
    std::uint8_t data_id_;
    std::uint8_t max_delta_counter_;
};

constexpr bool operator==(const E2EEventTypeDeployment& lhs, const E2EEventTypeDeployment& rhs) noexcept
{
    return (lhs.profile_ == rhs.profile_) && (lhs.data_id_ == rhs.data_id_) &&
           (lhs.max_delta_counter_ == rhs.max_delta_counter_);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_E2E_EVENT_TYPE_DEPLOYMENT_H
