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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_TEST_CONFIGURATION_TEST_RESOURCES_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_TEST_CONFIGURATION_TEST_RESOURCES_H

#include "score/mw/com/impl/configuration/lola_event_id.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_field_id.h"
#include "score/mw/com/impl/configuration/lola_field_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_field_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"

#include <score/optional.hpp>
#include <score/overload.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <unordered_map>

namespace score::mw::com::impl
{

LolaEventInstanceDeployment MakeDefaultLolaEventInstanceDeployment() noexcept;

LolaEventInstanceDeployment MakeLolaEventInstanceDeployment(
    const std::optional<std::uint16_t> number_of_sample_slots = 12U,
    const std::optional<std::uint8_t> max_subscribers = 13U,
    const std::optional<std::uint8_t> max_concurrent_allocations = 14U,
    const bool enforce_max_samples = true,
    std::uint8_t number_of_tracing_slots = 1U) noexcept;

LolaFieldInstanceDeployment MakeLolaFieldInstanceDeployment(
    const std::uint16_t max_samples = 12U,
    const std::optional<std::uint8_t> max_subscribers = 13U,
    const std::optional<std::uint8_t> max_concurrent_allocations = 14U,
    const bool enforce_max_samples = true,
    std::uint8_t number_of_tracing_slots = 1U) noexcept;

LolaServiceInstanceDeployment MakeLolaServiceInstanceDeployment(
    const score::cpp::optional<LolaServiceInstanceId> instance_id = 21U,
    const score::cpp::optional<std::size_t> shared_memory_size = 2000U) noexcept;

SomeIpServiceInstanceDeployment MakeSomeIpServiceInstanceDeployment(
    const score::cpp::optional<SomeIpServiceInstanceId> instance_id = 22U) noexcept;

LolaServiceTypeDeployment MakeLolaServiceTypeDeployment(const std::uint16_t service_id = 31U) noexcept;

class ConfigurationStructsFixture : public ::testing::Test
{
  public:
    void ExpectLolaEventInstanceDeploymentObjectsEqual(const LolaEventInstanceDeployment& lhs,
                                                       const LolaEventInstanceDeployment& rhs) const noexcept;

    void ExpectLolaFieldInstanceDeploymentObjectsEqual(const LolaFieldInstanceDeployment& lhs,
                                                       const LolaFieldInstanceDeployment& rhs) const noexcept;

    void ExpectSomeIpEventInstanceDeploymentObjectsEqual(const SomeIpEventInstanceDeployment& lhs,
                                                         const SomeIpEventInstanceDeployment& rhs) const noexcept;

    void ExpectSomeIpFieldInstanceDeploymentObjectsEqual(const SomeIpFieldInstanceDeployment& lhs,
                                                         const SomeIpFieldInstanceDeployment& rhs) const noexcept;

    void ExpectLolaServiceInstanceDeploymentObjectsEqual(const LolaServiceInstanceDeployment& lhs,
                                                         const LolaServiceInstanceDeployment& rhs) const noexcept;

    void ExpectSomeIpServiceInstanceDeploymentObjectsEqual(const SomeIpServiceInstanceDeployment& lhs,
                                                           const SomeIpServiceInstanceDeployment& rhs) const noexcept;

    void ExpectServiceInstanceDeploymentObjectsEqual(const ServiceInstanceDeployment& lhs,
                                                     const ServiceInstanceDeployment& rhs) const noexcept;

    void ExpectLolaServiceTypeDeploymentObjectsEqual(const LolaServiceTypeDeployment& lhs,
                                                     const LolaServiceTypeDeployment& rhs) const noexcept;

    void ExpectServiceTypeDeploymentObjectsEqual(const ServiceTypeDeployment& lhs,
                                                 const ServiceTypeDeployment& rhs) const noexcept;

    void ExpectServiceVersionTypeObjectsEqual(const ServiceVersionType& lhs,
                                              const ServiceVersionType& rhs) const noexcept;

    void ExpectServiceIdentifierTypeObjectsEqual(const ServiceIdentifierType& lhs,
                                                 const ServiceIdentifierType& rhs) const noexcept;

    void ExpectServiceInstanceIdObjectsEqual(const ServiceInstanceId& lhs, const ServiceInstanceId& rhs) const noexcept;

    void ExpectLolaServiceInstanceIdObjectsEqual(const LolaServiceInstanceId& lhs,
                                                 const LolaServiceInstanceId& rhs) const noexcept;

    void ExpectSomeIpServiceInstanceIdObjectsEqual(const SomeIpServiceInstanceId& lhs,
                                                   const SomeIpServiceInstanceId& rhs) const noexcept;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_TEST_CONFIGURATION_TEST_RESOURCES_H
