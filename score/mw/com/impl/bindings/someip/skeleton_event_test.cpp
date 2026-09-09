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

#include "score/mw/com/impl/bindings/someip/in_process_transport.h"
#include "score/mw/com/impl/bindings/someip/sample_allocatee_ptr.h"
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

namespace score::mw::com::impl::someip
{
namespace
{

using TestSampleType = std::uint32_t;

constexpr SomeIpServiceId kServiceId{1U};
constexpr SomeIpEventId kEventId{2U};
constexpr SomeIpServiceInstanceId::InstanceId kInstanceId{3U};
constexpr std::size_t kNumberOfSlots{2U};

const ElementFqId kEventFqId{kServiceId, kEventId, kInstanceId, ServiceElementType::EVENT};
const memory::DataTypeSizeInfo kSampleSizeInfo{sizeof(TestSampleType), alignof(TestSampleType)};

/// \brief Owns everything a someip::Skeleton needs to be constructed from a (SOME/IP) InstanceIdentifier.
class DeploymentStore final
{
  public:
    DeploymentStore()
        : instance_specifier_{InstanceSpecifier::Create("/someip_skeleton_event_test").value()},
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
        skeleton_ = std::make_unique<Skeleton>(deployment_store_.GetInstanceIdentifier(), transport_);
        unit_ = std::make_unique<SkeletonEvent>(*skeleton_,
                                                kEventFqId,
                                                "DummyEvent",
                                                kSampleSizeInfo,
                                                SkeletonEventProperties{kNumberOfSlots, 1U, true},
                                                impl::tracing::SkeletonEventTracingData{});
    }

    InitializeSampleCallback MakeInitializeSampleCallback() const noexcept
    {
        return InitializeSampleCallback{[](void* const sample_ptr) noexcept {
            score::cpp::ignore = new (sample_ptr) TestSampleType{};
        }};
    }

    TestSampleType GetLastSentValue() const noexcept
    {
        const auto payload = transport_.GetLastSentPayload(kEventFqId);
        EXPECT_EQ(payload.size(), sizeof(TestSampleType));
        TestSampleType value{};
        std::memcpy(&value, payload.data(), sizeof(TestSampleType));
        return value;
    }

    InProcessTransport transport_{};
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

TEST_F(SomeIpSkeletonEventFixture, PrepareOfferOffersTheEventOnTheTransport)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    // The transport only accepts sends for offered events, so a successful send proves the offer reached it.
    const TestSampleType value{42U};
    EXPECT_TRUE(unit_->Send(&value, std::nullopt, SampleAllocateeGuard{}).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, PrepareOfferInitializesEverySlot)
{
    std::size_t number_of_initialized_slots{0U};
    ASSERT_TRUE(unit_->PrepareOffer(InitializeSampleCallback{[&number_of_initialized_slots](void* const sample_ptr) {
                                        ++number_of_initialized_slots;
                                        score::cpp::ignore = new (sample_ptr) TestSampleType{};
                                    }})
                    .has_value());

    EXPECT_EQ(number_of_initialized_slots, kNumberOfSlots);
}

TEST_F(SomeIpSkeletonEventFixture, SendWithCopyTransmitsThePayload)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{0xDEADBEEFU};

    ASSERT_TRUE(unit_->Send(&value, std::nullopt, SampleAllocateeGuard{}).has_value());

    EXPECT_EQ(GetLastSentValue(), value);
}

TEST_F(SomeIpSkeletonEventFixture, SendWithAllocateTransmitsThePayload)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
    ASSERT_TRUE(allocate_result.has_value());
    auto sample = std::move(allocate_result).value();
    *static_cast<TestSampleType*>(sample.Get()) = 7U;

    ASSERT_TRUE(unit_->Send(std::move(sample), std::nullopt).has_value());

    EXPECT_EQ(GetLastSentValue(), 7U);
}

TEST_F(SomeIpSkeletonEventFixture, SendWithCopyDoesNotExhaustSlots)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    // More sends than there are slots: every Send() has to return its slot, otherwise this runs out of slots.
    for (TestSampleType value = 0U; value < (kNumberOfSlots * 3U); ++value)
    {
        ASSERT_TRUE(unit_->Send(&value, std::nullopt, SampleAllocateeGuard{}).has_value()) << "at value " << value;
    }

    EXPECT_EQ(GetLastSentValue(), (kNumberOfSlots * 3U) - 1U);
}

TEST_F(SomeIpSkeletonEventFixture, AllocateFailsWhenAllSlotsAreHeldByTheUser)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    std::vector<impl::SampleAllocateePtr<void>> held_samples{};
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
        ASSERT_TRUE(allocate_result.has_value());
        held_samples.push_back(std::move(allocate_result).value());
    }

    EXPECT_FALSE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, DestroyingAnUnsentSampleReturnsItsSlot)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    {
        std::vector<impl::SampleAllocateePtr<void>> held_samples{};
        for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
        {
            auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
            ASSERT_TRUE(allocate_result.has_value());
            held_samples.push_back(std::move(allocate_result).value());
        }
        ASSERT_FALSE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
    }

    EXPECT_TRUE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, SendReturnsExactlyOneSlot)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    // Hold every slot but one, then send the remaining one. If Send() returned its slot more than once, one of the
    // still-held slots would be wrongly marked free and the Allocate() below would wrongly succeed.
    std::vector<impl::SampleAllocateePtr<void>> held_samples{};
    for (std::size_t slot = 0U; slot < (kNumberOfSlots - 1U); ++slot)
    {
        auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
        ASSERT_TRUE(allocate_result.has_value());
        held_samples.push_back(std::move(allocate_result).value());
    }

    const TestSampleType value{42U};
    ASSERT_TRUE(unit_->Send(&value, std::nullopt, SampleAllocateeGuard{}).has_value());

    // Exactly one slot became free again, so exactly one further allocation must succeed.
    auto first_allocate_result = unit_->Allocate(SampleAllocateeGuard{});
    ASSERT_TRUE(first_allocate_result.has_value());
    EXPECT_FALSE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, SendReturnsItsSlotEvenWhenTheTransportFails)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    // Withdrawing the event on the transport directly (without going through PrepareStopOffer) makes the next
    // SendEvent() fail while the event itself still considers itself offered.
    transport_.StopOfferEvent(kEventFqId);

    const TestSampleType value{42U};
    ASSERT_FALSE(unit_->Send(&value, std::nullopt, SampleAllocateeGuard{}).has_value());

    // All slots must still be available, i.e. the failed send must not have leaked its slot.
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        EXPECT_TRUE(unit_->Allocate(SampleAllocateeGuard{}).has_value()) << "at slot " << slot;
    }
}

TEST_F(SomeIpSkeletonEventFixture, PrepareStopOfferWithdrawsTheEvent)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    unit_->PrepareStopOffer();

    const TestSampleType value{42U};
    EXPECT_FALSE(unit_->Send(&value, std::nullopt, SampleAllocateeGuard{}).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, ReOfferingReusesTheExistingStorageWithoutReInitializingSlots)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    unit_->PrepareStopOffer();

    std::size_t number_of_initialized_slots{0U};
    ASSERT_TRUE(unit_->PrepareOffer(InitializeSampleCallback{[&number_of_initialized_slots](void* const sample_ptr) {
                                        ++number_of_initialized_slots;
                                        score::cpp::ignore = new (sample_ptr) TestSampleType{};
                                    }})
                    .has_value());

    EXPECT_EQ(number_of_initialized_slots, 0U);
}

TEST_F(SomeIpSkeletonEventFixture, NotifyBeforeOfferFails)
{
    EXPECT_FALSE(unit_->Notify().has_value());
}

TEST_F(SomeIpSkeletonEventFixture, NotifyIsForwardedToTheTransport)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    ASSERT_TRUE(unit_->Notify().has_value());

    EXPECT_EQ(transport_.GetNotificationCount(kEventFqId), 1U);
}

TEST_F(SomeIpSkeletonEventFixture, GetLatestSampleIsNotSupported)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    EXPECT_FALSE(unit_->GetLatestSample(QualityType::kASIL_QM).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, SettingTheReceiveHandlerRegistrationChangedHandlerReportsCurrentState)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    std::optional<bool> reported_state{};
    ASSERT_TRUE(unit_
                    ->SetReceiveHandlerRegistrationChangedHandler(
                        ReceiveHandlerRegistrationChangedCallback{[&reported_state](const bool has_handlers) noexcept {
                            reported_state = has_handlers;
                        }})
                    .has_value());

    ASSERT_TRUE(reported_state.has_value());
    EXPECT_FALSE(reported_state.value());

    ASSERT_TRUE(transport_.Subscribe(kEventFqId).has_value());
    ASSERT_TRUE(unit_
                    ->SetReceiveHandlerRegistrationChangedHandler(
                        ReceiveHandlerRegistrationChangedCallback{[&reported_state](const bool has_handlers) noexcept {
                            reported_state = has_handlers;
                        }})
                    .has_value());
    EXPECT_TRUE(reported_state.value());
}

TEST_F(SomeIpSkeletonEventFixture, UnsettingTheReceiveHandlerRegistrationChangedHandlerSucceeds)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    EXPECT_TRUE(unit_->UnsetReceiveHandlerRegistrationChangedHandler().has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::someip
