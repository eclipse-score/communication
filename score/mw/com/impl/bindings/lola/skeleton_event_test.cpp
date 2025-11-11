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

}  // namespace
}  // namespace score::mw::com::impl::lola
