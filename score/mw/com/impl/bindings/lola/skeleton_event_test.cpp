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
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_test_resources.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_event_test_resources.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"

#include "score/filesystem/filesystem.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;

MATCHER(SubscribeSucceeded, "")
{
    return arg == SubscribeResult::kSuccess;
}

using SkeletonEventGetBindingTypeFixture = SkeletonEventFixture;
TEST_F(SkeletonEventGetBindingTypeFixture, GetBindingType)
{
    const bool enforce_max_samples{true};
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples_, max_subscribers_, enforce_max_samples);

    EXPECT_EQ(skeleton_event_->GetBindingType(), BindingType::kLoLa);
}

using SkeletonEventAllocateFixture = SkeletonEventFixture;
TEST_F(SkeletonEventAllocateFixture, CannotAllocateBeforeCallingOffer)
{
    const bool enforce_max_samples{true};

    // Given an un-offered event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples_, max_subscribers_, enforce_max_samples);

    // When allocating and sending the allocated event
    auto ptr = skeleton_event_->Allocate();

    // Then we should get a nullptr
    EXPECT_FALSE(ptr);
}

TEST_F(SkeletonEventAllocateFixture, AllocateErrorLeadsToNullptr)
{
    RecordProperty("Verifies", "SCR-21840368, SCR-17434933, SCR-5899090, SSR-6225206");
    RecordProperty("Description",
                   "Checks that allocation algo aborts correctly (req. SSR-6225206) and an is returned on "
                   "allocation error (req. SCR-21840368, SCR-17434933) and that the number of slots is a "
                   "configurable param handed over in ctor (req. SCR-5899090)");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};

    // Given an un-offered event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples_, max_subscribers_, enforce_max_samples);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();
    std::vector<impl::SampleAllocateePtr<test::TestSampleType>> pointer_collection{max_samples_};
    for (std::size_t counter = 0; counter < max_samples_; ++counter)
    {
        auto allocate_result = skeleton_event_->Allocate();
        ASSERT_TRUE(allocate_result.has_value());
        pointer_collection[counter] = std::move(allocate_result).value();
    }

    // since we have a ASIL_B skeleton, expect, that it disconnects QM clients, when allocation fails and QM consumers
    // have not yet been disconnected via StopOffering the QM part of its service.
    EXPECT_CALL(service_discovery_mock_, StopOfferService(_, IServiceDiscovery::QualityTypeSelector::kAsilQm)).Times(1);

    // When allocating a sixth (max_samples_ + 1) slot
    auto allocate_result = skeleton_event_->Allocate();

    // Then the slot cannot be allocated
    ASSERT_FALSE(allocate_result.has_value());
    EXPECT_EQ(allocate_result.error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonEventAllocateFixture, SkeletonEventWithNotMaxSamplesEnforcementAllocateErrorLeadsToError)
{
    const bool enforce_max_samples{false};

    // Given an un-offered event in an offered service which does not enforce max samples
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples_, max_subscribers_, enforce_max_samples);

    // When offering the event
    skeleton_event_->PrepareOffer();
    std::vector<impl::SampleAllocateePtr<test::TestSampleType>> pointer_collection{max_samples_};
    for (std::size_t counter = 0; counter < max_samples_; ++counter)
    {
        auto allocate_result = skeleton_event_->Allocate();
        ASSERT_TRUE(allocate_result.has_value());
        pointer_collection[counter] = std::move(allocate_result).value();
    }

    // since we have a ASIL_B skeleton, expect, that it disconnects QM clients, when allocation fails and QM consumers
    // have not yet been disconnected via StopOffering the QM part of its service.
    EXPECT_CALL(service_discovery_mock_, StopOfferService(_, IServiceDiscovery::QualityTypeSelector::kAsilQm)).Times(1);

    // When allocating a sixth slot
    auto allocate_result = skeleton_event_->Allocate();

    // Then the slot cannot be allocated
    ASSERT_FALSE(allocate_result.has_value());
    EXPECT_EQ(allocate_result.error(), ComErrc::kBindingFailure);
}

TEST_F(SkeletonEventAllocateFixture, AllocateReturnsUniquePointersForMultipleCalls)
{
    RecordProperty("Description", "Checks that multiple calls to Allocate() return unique memory pointers.");
    RecordProperty("TestType", "Unit Test");

    const bool enforce_max_samples{true};
    const size_t num_allocations = 3;
    ASSERT_LE(num_allocations, max_samples_);

    // Given an offered event
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples_, max_subscribers_, enforce_max_samples);
    skeleton_event_->PrepareOffer();

    // When allocating multiple samples without sending them
    std::vector<impl::SampleAllocateePtr<test::TestSampleType>> allocated_pointers;
    std::vector<void*> raw_pointers;

    for (size_t i = 0; i < num_allocations; ++i)
    {
        auto alloc_result = skeleton_event_->Allocate();
        ASSERT_TRUE(alloc_result.has_value()) << "Allocation " << i << " failed";
        
        // Store the raw pointer to check for uniqueness
        raw_pointers.push_back(alloc_result.value().Get());

        // Keep the SampleAllocateePtr alive to keep the slot busy
        allocated_pointers.push_back(std::move(alloc_result.value()));
    }

    // Then all allocated raw pointers should be unique
    std::sort(raw_pointers.begin(), raw_pointers.end());
    auto it = std::unique(raw_pointers.begin(), raw_pointers.end());
    EXPECT_EQ(it, raw_pointers.end()) << "Duplicate memory addresses were allocated.";
}

using SkeletonEventPrepareOfferFixture = SkeletonEventFixture;
TEST_F(SkeletonEventPrepareOfferFixture, SubscriptionsAcceptedIfMaxSamplesCanBeProvided)
{
    RecordProperty("Verifies", "SCR-7088394, SCR-21269964, SCR-14137270, SCR-17292398, SCR-14033248");
    RecordProperty("Description",
                   "Checks that a subscription will be accepted by the provider if the requested max_sample_count can "
                   "be provided.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};

    // Given a valid skeleton event with max. sample count of 5, which as per default enforces the max sample count
    // in subscriptions.
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers_, enforce_max_samples);

    // When offering a skeleton event
    skeleton_event_->PrepareOffer();

    auto* const event_control = GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM);
    ASSERT_NE(event_control, nullptr);
    auto& subscription_control = event_control->subscription_control;

    // When a proxy tries to subscribe with 3 samples
    // Then the subscription is accepted
    EXPECT_THAT(subscription_control.Subscribe(3U), SubscribeSucceeded());

    // and when a second proxy tries to subscribe with 1 sample
    // Then the subscription is accepted
    EXPECT_THAT(subscription_control.Subscribe(1U), SubscribeSucceeded());
}

TEST_F(SkeletonEventPrepareOfferFixture, SubscriptionRejectedIfMaxSubscriptionCountOverflowOccurs)
{
    RecordProperty("Verifies", "SCR-7088394, SCR-21269964, SCR-14137270, SCR-17292398, SCR-14033248");
    RecordProperty("Description",
                   "Checks that a subscription will be rejected if an 'over-subscription' occurs on the skeleton.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};

    // Given a valid skeleton event with max. sample count of 5, which as per default enforces the max sample count
    // in subscriptions.
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers_, enforce_max_samples);

    // When offering a skeleton event
    skeleton_event_->PrepareOffer();

    auto* const event_control = GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM);
    ASSERT_NE(event_control, nullptr);
    auto& subscription_control = event_control->subscription_control;

    // When a proxy tries to subscribe with 3 samples
    // Then the subscription is accepted
    EXPECT_THAT(subscription_control.Subscribe(3U), SubscribeSucceeded());

    // and when a second proxy tries to subscribe with 3 samples
    // Then the subscription is rejected as the total number of requested samples exceeds the maximum
    EXPECT_THAT(subscription_control.Subscribe(3U), Not(SubscribeSucceeded()));
}

TEST_F(SkeletonEventPrepareOfferFixture, SubscriptionAcceptedIfOversubscriptionAllowedOnConstruction)
{
    RecordProperty("Verifies", "SCR-7088394, SCR-21269964, SCR-14137270, SCR-17292398, SCR-14033248");
    RecordProperty("Description",
                   "Checks that a skeleton event allows 'over-subscription' in case it is constructed accordingly");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{false};
    const std::size_t max_samples{5U};

    // Given a valid skeleton event with max. sample count of 5, which does NOT enforce maxSamples in subscriptions.
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers_, enforce_max_samples);

    // When offering a skeleton event
    skeleton_event_->PrepareOffer();

    auto* const event_control = GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM);
    ASSERT_NE(event_control, nullptr);
    auto& subscription_control = event_control->subscription_control;

    // When a proxy tries to subscribe with 3 samples
    // Then the subscription is accepted
    EXPECT_THAT(subscription_control.Subscribe(3U), SubscribeSucceeded());

    // and when a second proxy tries to subscribe with 3 samples
    // Then the subscription is accepted as the maximum number of samples is not enforced
    EXPECT_THAT(subscription_control.Subscribe(3U), SubscribeSucceeded());
}

TEST_F(SkeletonEventPrepareOfferFixture, SubscriptionRejectedIfNumberOfSubscriberExceedsLimit)
{
    RecordProperty("Verifies", "SCR-7088394, SCR-21269964, SCR-14137270, SCR-17292398, SCR-14033248");
    RecordProperty("Description",
                   "Checks that a subscription will be rejected if the number of subscriptions is already equal or "
                   "greater than the max number of subscribers allowed.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};

    // Given a valid skeleton event with max. sample count of 5 and max number of subscribers of 3, which as per default
    // enforces the max sample count in subscriptions.
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers_, enforce_max_samples);

    // When offering a skeleton event
    skeleton_event_->PrepareOffer();

    auto* const event_control = GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM);
    ASSERT_NE(event_control, nullptr);
    auto& subscription_control = event_control->subscription_control;

    // When a proxy tries to subscribe with 1 sample
    // Then the subscription is accepted
    EXPECT_THAT(subscription_control.Subscribe(1U), SubscribeSucceeded());

    // and when another proxy tries to subscribe with 1 sample
    // Then the subscription is accepted
    EXPECT_THAT(subscription_control.Subscribe(1U), SubscribeSucceeded());

    // and when another proxy tries to subscribe with 1 sample
    // Then the subscription is accepted
    EXPECT_THAT(subscription_control.Subscribe(1U), SubscribeSucceeded());

    // but when another proxy tries to subscribe with 1 sample
    // Then the subscription is rejected as the maximum number of subscribers has been exceeded
    EXPECT_THAT(subscription_control.Subscribe(1U), Not(SubscribeSucceeded()));
}

TEST_F(SkeletonEventPrepareOfferFixture, UnsubscribeIncreasesAvailableSampleSlots)
{
    RecordProperty("Verifies", "SCR-14033377, SCR-17292399, SCR-14137271, SCR-21286218");
    RecordProperty("Description",
                   "The available sample count will be incremented when an unsubscribe message is received.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};

    // Given a valid skeleton event with max. sample count of 5
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers_, enforce_max_samples);

    // When offering a skeleton event
    skeleton_event_->PrepareOffer();

    auto* const event_control = GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM);
    ASSERT_NE(event_control, nullptr);
    auto& subscription_control = event_control->subscription_control;

    // When a proxy tries to subscribe with 3 samples
    // Then the subscription is accepted
    EXPECT_THAT(subscription_control.Subscribe(3U), SubscribeSucceeded());

    // and a proxy tries to unsubscribe with 3 samples
    // Then the unsubscribe is accepted
    subscription_control.Unsubscribe(3U);

    // and when another proxy tries to subscribe with 3 samples
    // Then the subscription is accepted as the unsubscribe freed up 3 samples
    EXPECT_THAT(subscription_control.Subscribe(3U), SubscribeSucceeded());
}

using SkeletonEventPrepareStopOfferFixture = SkeletonEventFixture;
TEST_F(SkeletonEventPrepareStopOfferFixture, StopOfferSkeletonEvent)
{
    const bool enforce_max_samples{true};

    // Given an un-offered event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples_, max_subscribers_, enforce_max_samples);

    // When stop offering a skeleton event
    skeleton_event_->PrepareStopOffer();
}

using SkeletonEventTimestampFixture = SkeletonEventFixture;
TEST_F(SkeletonEventTimestampFixture, SendUpdatesTimestampInControlData)
{
    // GIVEN a skeleton event that is offered
    const bool enforce_max_samples{true};
    impl::tracing::SkeletonEventTracingData tracing_data{};
    InitialiseSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples_, max_subscribers_, enforce_max_samples, tracing_data);

    skeleton_event_->PrepareOffer();

    // WHEN we allocate and send a first sample
    auto first_allocated_slot_result = skeleton_event_->Allocate();
    ASSERT_TRUE(first_allocated_slot_result.has_value());
    auto first_allocated_slot = std::move(first_allocated_slot_result).value();

    const impl::SampleAllocateePtrView<test::TestSampleType> first_view{first_allocated_slot};
    auto* first_lola_ptr = first_view.template As<lola::SampleAllocateePtr<test::TestSampleType>>();
    auto first_slot_indicator = first_lola_ptr->GetReferencedSlot();

    auto first_send_result = skeleton_event_->Send(std::move(first_allocated_slot), score::cpp::nullopt);
    ASSERT_TRUE(first_send_result.has_value());

    // THEN its timestamp should be a valid, non-zero value
    const EventSlotStatus first_final_slot_status{first_slot_indicator.GetSlotQM().load()};
    // AND the first timestamp should be 2, as it's the first one after initialization.
    const auto first_timestamp = first_final_slot_status.GetTimeStamp();
    EXPECT_EQ(first_timestamp, 2U);

    // AND WHEN we allocate and send a second sample
    auto second_allocated_slot_result = skeleton_event_->Allocate();
    ASSERT_TRUE(second_allocated_slot_result.has_value());
    auto second_allocated_slot = std::move(second_allocated_slot_result).value();

    const impl::SampleAllocateePtrView<test::TestSampleType> second_view{second_allocated_slot};
    auto* second_lola_ptr = second_view.template As<lola::SampleAllocateePtr<test::TestSampleType>>();
    auto second_slot_indicator = second_lola_ptr->GetReferencedSlot();
    auto second_send_result = skeleton_event_->Send(std::move(second_allocated_slot), score::cpp::nullopt);
    ASSERT_TRUE(second_send_result.has_value());

    // THEN its timestamp should be exactly one greater than the first one
    const EventSlotStatus second_final_slot_status{second_slot_indicator.GetSlotQM().load()};
    const auto second_timestamp = second_final_slot_status.GetTimeStamp();
    EXPECT_EQ(second_timestamp, first_timestamp + 1U)
        << "The second timestamp should be exactly one greater than the first.";
}

}  // namespace
}  // namespace score::mw::com::impl::lola
