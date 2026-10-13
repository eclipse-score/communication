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
#include "score/mw/com/impl/bindings/someip/skeleton_event.h"

#include "score/mw/com/impl/bindings/someip/skeleton.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace score::mw::com::impl::someip
{
namespace
{

using TestSampleType = std::uint32_t;

constexpr SomeIpServiceId kServiceId{1U};
constexpr SomeIpEventId kEventId{2U};
constexpr SomeIpServiceInstanceId::InstanceId kInstanceId{3U};

const ElementFqId kEventFqId{kServiceId, kEventId, kInstanceId, ServiceElementType::EVENT};
const memory::DataTypeSizeInfo kSampleSizeInfo{sizeof(TestSampleType), alignof(TestSampleType)};

/// \brief Owns everything a someip::Skeleton needs to be constructed from a (SOME/IP) InstanceIdentifier.
class DeploymentStore final
{
  public:
    DeploymentStore()
        : instance_specifier_{InstanceSpecifier::Create(std::string{"/someip_skeleton_event_test"}).value()},
          service_identifier_{make_ServiceIdentifierType("SomeIpSkeletonEventTestService", 1U, 0U)},
          service_type_deployment_{std::make_unique<ServiceTypeDeployment>(
              SomeIpServiceTypeDeployment{kServiceId, {{"DummyEvent", kEventId}}})},
          service_instance_deployment_{std::make_unique<ServiceInstanceDeployment>(
              service_identifier_,
              SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{kInstanceId}},
              QualityType::kASIL_QM,
              instance_specifier_)}
    {
    }

    InstanceIdentifier GetInstanceIdentifier() const noexcept
    {
        return make_InstanceIdentifier(*service_instance_deployment_, *service_type_deployment_);
    }

  private:
    InstanceSpecifier instance_specifier_;
    ServiceIdentifierType service_identifier_;
    std::unique_ptr<ServiceTypeDeployment> service_type_deployment_;
    std::unique_ptr<ServiceInstanceDeployment> service_instance_deployment_;
};

class SomeIpSkeletonEventFixture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        skeleton_ = std::make_unique<Skeleton>(deployment_store_.GetInstanceIdentifier());
        unit_ = std::make_unique<SkeletonEvent>(*skeleton_,
                                                kEventFqId,
                                                "DummyEvent",
                                                kSampleSizeInfo,
                                                SkeletonEventProperties{2U, 1U, true},
                                                impl::tracing::SkeletonEventTracingData{});
    }

    DeploymentStore deployment_store_{};
    std::unique_ptr<Skeleton> skeleton_{};
    std::unique_ptr<SkeletonEvent> unit_{};
};

TEST_F(SomeIpSkeletonEventFixture, GetBindingTypeReturnsSomeIp)
{
    EXPECT_EQ(unit_->GetBindingType(), BindingType::kSomeIp);
}

TEST_F(SomeIpSkeletonEventFixture, GetEventDataTypeSizeInfoReturnsConfiguredSizeInfo)
{
    EXPECT_EQ(unit_->GetEventDataTypeSizeInfo().Size(), kSampleSizeInfo.Size());
    EXPECT_EQ(unit_->GetEventDataTypeSizeInfo().Alignment(), kSampleSizeInfo.Alignment());
}

TEST_F(SomeIpSkeletonEventFixture, AllocateBeforeOfferFails)
{
    const auto result = unit_->Allocate(SampleAllocateeGuard{});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kNotOffered);
}

TEST_F(SomeIpSkeletonEventFixture, PrepareOfferFailsWithoutTransport)
{
    const auto result = unit_->PrepareOffer(std::nullopt);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
    const auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
    ASSERT_FALSE(allocate_result.has_value());
    EXPECT_EQ(allocate_result.error(), ComErrc::kNotOffered);
}

TEST_F(SomeIpSkeletonEventFixture, SendFailsWithoutTransport)
{
    const auto result = unit_->Send(impl::SampleAllocateePtr<void>{}, std::nullopt);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(SomeIpSkeletonEventFixture, PrepareStopOfferLeavesEventUnoffered)
{
    unit_->PrepareStopOffer();

    const auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
    ASSERT_FALSE(allocate_result.has_value());
    EXPECT_EQ(allocate_result.error(), ComErrc::kNotOffered);
}

TEST_F(SomeIpSkeletonEventFixture, GetLatestSampleIsNotSupported)
{
    EXPECT_FALSE(unit_->GetLatestSample(QualityType::kASIL_QM).has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::someip
