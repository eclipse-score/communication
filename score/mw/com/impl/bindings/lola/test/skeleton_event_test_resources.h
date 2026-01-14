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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_TEST_RESOURCES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_TEST_RESOURCES_H

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_mock.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/service_discovery_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{

class SkeletonEventFixture : public SkeletonMockedMemoryFixture
{
  public:
    SkeletonEventFixture();

    void InitialiseSkeletonEvent(ElementFqId element_fq_id,
                                 const std::string& service_element_name,
                                 const std::size_t max_samples,
                                 const std::uint8_t max_subscribers,
                                 bool enforce_max_samples = true,
                                 impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data = {});

    InstanceIdentifier GetValidInstanceIdentifier();

    EventControl* GetEventControl(ElementFqId element_fq_id, QualityType quality_type) const noexcept;

    const std::uint8_t max_samples_{5U};
    std::uint8_t max_subscribers_{3U};

    const std::string service_type_name_{"foo"};
    const ElementFqId fake_element_fq_id_{1, 1, 1, ServiceElementType::EVENT};
    const std::string fake_event_name_{"dummy"};
    const InstanceSpecifier instance_specifier_{
        InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value()};

    LolaServiceInstanceDeployment binding_info_{CreateLolaServiceInstanceDeployment(
        test::kDefaultLolaInstanceId,
        {{fake_event_name_, LolaEventInstanceDeployment{max_samples_, max_subscribers_, 1U, true, 0}}},
        {},
        {},
        {},
        {},
        test::kConfiguredDeploymentShmSize)};

    /// \brief A very basic (Lola) ServiceTypeDeployment, which just contains a service-id and NO events at all!
    /// \details For some of the basic tests, this is sufficient and since services without events are a valid use
    /// case
    ///          (at least later, when we also support fields/service-methods).
    const ServiceTypeDeployment valid_type_deployment_{CreateTypeDeployment(2U, {{fake_event_name_, 42U}})};

    /// \brief A very basic (Lola) ASIL-B ServiceInstanceDeployment, which relates to the
    ///        valid_type_deployment_ and has a shm-size configuration of 500.
    /// \note same setup as valid_qm_instance_deployment, but ASIL-QM and ASIL-B.
    ServiceInstanceDeployment valid_asil_instance_deployment_{make_ServiceIdentifierType(service_type_name_),
                                                              binding_info_,
                                                              QualityType::kASIL_B,
                                                              instance_specifier_};

    std::unique_ptr<SkeletonEvent<test::TestSampleType>> skeleton_event_;

    /// mocks used by test
    MessagePassingServiceMock message_passing_service_mock_{};
    ServiceDiscoveryMock service_discovery_mock_{};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_EVENT_TEST_RESOURCES_H
