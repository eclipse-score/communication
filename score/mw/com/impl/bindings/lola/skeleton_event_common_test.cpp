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

#include "score/mw/com/impl/bindings/lola/skeleton_event_common.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_event_test_resources.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnRef;

class SkeletonEventCommonFixture : public SkeletonEventFixture
{
  public:
    SkeletonEventCommonFixture()
    {
        ON_CALL(runtime_mock_, GetServiceDiscovery()).WillByDefault(ReturnRef(service_discovery_mock_));
    }

    void InitialiseSkeletonEventCommon(const ElementFqId element_fq_id,
                                       const std::string& service_element_name,
                                       const std::size_t max_samples,
                                       const std::uint8_t max_subscribers,
                                       const bool enforce_max_samples,
                                       const QualityType quality_type,
                                       impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data)
    {
        // We defer initialisation of the Skeleton to InitialiseSkeletonEventCommon to allow test fixtures to set any
        // mocked expectations before creating the skeleton.
        InitialiseSkeleton(GetValidInstanceIdentifierForEventCommon(quality_type));

        SkeletonBinding::SkeletonEventBindings events{};
        SkeletonBinding::SkeletonFieldBindings fields{};
        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback{};

        std::ignore = skeleton_->PrepareOffer(events, fields, std::move(register_shm_object_trace_callback));

        skeleton_event_ = std::make_unique<SkeletonEvent<test::TestSampleType>>(
            *skeleton_,
            element_fq_id,
            service_element_name,
            SkeletonEventProperties{max_samples, max_subscribers, enforce_max_samples},
            skeleton_event_tracing_data);

        // Call PrepareOffer on the skeleton event to trigger Skeleton::Register, which populates
        // the event_controls_ maps in ServiceDataControl for both QM and (if ASIL-B) ASIL-B.
        std::ignore = skeleton_event_->PrepareOffer();

        skeleton_event_common_ =
            std::make_unique<score::mw::com::impl::lola::SkeletonEventCommon<test::TestSampleType>>(
                *skeleton_,
                event_name_,
                SkeletonEventProperties{max_samples, max_subscribers, enforce_max_samples},
                element_fq_id,
                skeleton_event_tracing_data);
    }

  protected:
    std::string_view event_name_{"test_event"};
    std::unique_ptr<SkeletonEventCommon<test::TestSampleType>> skeleton_event_common_;

    InstanceIdentifier GetValidInstanceIdentifierForEventCommon(QualityType quality_type) const
    {
        if (quality_type == QualityType::kASIL_B)
        {
            return make_InstanceIdentifier(valid_asil_instance_deployment_, valid_type_deployment_);
        }
        else
        {
            return make_InstanceIdentifier(valid_qm_instance_deployment_, valid_type_deployment_);
        }
    }

  private:
    ServiceInstanceDeployment valid_qm_instance_deployment_{make_ServiceIdentifierType(service_type_name_),
                                                            binding_info_,
                                                            QualityType::kASIL_QM,
                                                            instance_specifier_};
};

TEST_F(SkeletonEventCommonFixture, RegisterEventNotificationCallbacksForAsilBTriggersMessagePassingRegistration)
{
    const bool enforce_max_samples{true};
    InitialiseSkeletonEventCommon(fake_element_fq_id_,
                                  fake_event_name_,
                                  max_samples_,
                                  max_subscribers_,
                                  enforce_max_samples,
                                  QualityType::kASIL_B,
                                  {});

    auto* const event_control_qm = GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM);
    auto* const event_control_asil_b = GetEventControl(fake_element_fq_id_, QualityType::kASIL_B);
    ASSERT_NE(event_control_qm, nullptr);
    ASSERT_NE(event_control_asil_b, nullptr);

    // Expect that RegisterEventNotificationExistenceChangedCallback is called once per quality type with correct ASIL
    // level and element ID
    EXPECT_CALL(message_passing_mock_,
                RegisterEventNotificationExistenceChangedCallback(impl::QualityType::kASIL_B, fake_element_fq_id_, _))
        .Times(1);
    EXPECT_CALL(message_passing_mock_,
                RegisterEventNotificationExistenceChangedCallback(impl::QualityType::kASIL_QM, fake_element_fq_id_, _))
        .Times(1);
    // ... when PrepareOfferCommon is called
    skeleton_event_common_->PrepareOfferCommon(*event_control_qm, event_control_asil_b);
}

TEST_F(SkeletonEventCommonFixture, UnregisterEventNotificationCallbacksForAsilBTriggersMessagePassingUnregistration)
{
    const bool enforce_max_samples{true};
    InitialiseSkeletonEventCommon(fake_element_fq_id_,
                                  fake_event_name_,
                                  max_samples_,
                                  max_subscribers_,
                                  enforce_max_samples,
                                  QualityType::kASIL_B,
                                  {});

    auto* const event_control_qm = GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM);
    auto* const event_control_asil_b = GetEventControl(fake_element_fq_id_, QualityType::kASIL_B);
    ASSERT_NE(event_control_qm, nullptr);
    ASSERT_NE(event_control_asil_b, nullptr);

    // Expect that RegisterEventNotificationExistenceChangedCallback is called once per quality type with correct ASIL
    // level and element ID
    EXPECT_CALL(message_passing_mock_,
                UnregisterEventNotificationExistenceChangedCallback(impl::QualityType::kASIL_B, fake_element_fq_id_))
        .Times(1);
    EXPECT_CALL(message_passing_mock_,
                UnregisterEventNotificationExistenceChangedCallback(impl::QualityType::kASIL_QM, fake_element_fq_id_))
        .Times(1);
    // ... when PrepareStopOfferCommon is called
    skeleton_event_common_->PrepareStopOfferCommon();
}

}  // namespace

}  // namespace score::mw::com::impl::lola
