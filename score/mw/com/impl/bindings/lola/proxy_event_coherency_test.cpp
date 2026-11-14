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

#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_fixture.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/configuration/coherency_type.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

/// \brief Tests for a CoherencyType::kCoherent ProxyEvent, covering both skeleton-side counterparts it is compatible
/// with: a plain CoherencyType::kCoherent skeleton-side event, and a CoherencyType::kMixed one (see
/// Proxy::GetEventDataControlCoherentOrMixed()).
using LolaProxyEventCoherencyFixture = LolaProxyEventFixture;

TEST_F(LolaProxyEventCoherencyFixture, CoherentProxyReadingCoherentSkeletonReceivesSamplesInOrder)
{
    this->RecordProperty("Description",
                         "Checks that a CoherencyType::kCoherent ProxyEvent can subscribe to and receive samples "
                         "from a CoherencyType::kCoherent skeleton-side event.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a skeleton-side event configured as CoherencyType::kCoherent
    this->InitialiseDummySkeletonEventCoherent(
        this->element_fq_id_coherency_,
        SkeletonEventProperties{this->max_num_slots_, 0U, 0U, false, this->max_subscribers_, true});

    // and a CoherencyType::kCoherent ProxyEvent subscribed to it, with two samples already published
    std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> values_to_send{
        {kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}};
    this->GivenAProxyEvent(this->element_fq_id_coherency_, this->event_name_coherency_, CoherencyType::kCoherent)
        .ThatIsSubscribedWithMaxSamples(5U)
        .WithSkeletonEventDataCoherent(values_to_send);

    // When calling GetNewSamples with a max_samples higher than the number of samples available
    std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> received_samples{};
    const auto num_callbacks_result = this->GetNewSamples(
        [&received_samples](impl::SamplePtr<void> sample, const tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);
            received_samples.emplace_back(GetSamplePtrValue(sample.get()), timestamp);
        },
        5U);

    // Then both samples are received, in the same order they were published
    ASSERT_TRUE(num_callbacks_result.has_value());
    EXPECT_EQ(num_callbacks_result.value(), 2U);
    EXPECT_EQ(values_to_send, received_samples);
}

TEST_F(LolaProxyEventCoherencyFixture, CoherentProxyReadingMixedSkeletonReceivesSamplesInOrder)
{
    this->RecordProperty("Description",
                         "Checks that a CoherencyType::kCoherent ProxyEvent can subscribe to and receive samples "
                         "from a CoherencyType::kMixed skeleton-side event.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a skeleton-side event configured as CoherencyType::kMixed
    this->InitialiseDummySkeletonEventMixed(
        this->element_fq_id_coherency_,
        SkeletonEventProperties{this->max_num_slots_, 0U, 0U, false, this->max_subscribers_, true});

    // and a CoherencyType::kCoherent ProxyEvent subscribed to it, with two samples already published
    std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> values_to_send{
        {kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}};
    this->GivenAProxyEvent(this->element_fq_id_coherency_, this->event_name_coherency_, CoherencyType::kCoherent)
        .ThatIsSubscribedWithMaxSamples(5U)
        .WithSkeletonEventDataMixed(values_to_send);

    // When calling GetNewSamples with a max_samples higher than the number of samples available
    std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> received_samples{};
    const auto num_callbacks_result = this->GetNewSamples(
        [&received_samples](impl::SamplePtr<void> sample, const tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);
            received_samples.emplace_back(GetSamplePtrValue(sample.get()), timestamp);
        },
        5U);

    // Then both samples are received, in the same order they were published
    ASSERT_TRUE(num_callbacks_result.has_value());
    EXPECT_EQ(num_callbacks_result.value(), 2U);
    EXPECT_EQ(values_to_send, received_samples);
}

TEST_F(LolaProxyEventCoherencyFixture, CoherentProxyReadingCoherentSkeletonDoesNotReceiveSamplesFromThePast)
{
    this->RecordProperty("Description",
                         "Checks that a CoherencyType::kCoherent ProxyEvent does not re-receive samples which are "
                         "older than the newest one already seen in a previous GetNewSamples() call.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a skeleton-side event configured as CoherencyType::kCoherent, with one sample already published
    this->InitialiseDummySkeletonEventCoherent(
        this->element_fq_id_coherency_,
        SkeletonEventProperties{this->max_num_slots_, 0U, 0U, false, this->max_subscribers_, true});
    constexpr EventSlotStatus::EventTimeStamp input_timestamp{17U};
    this->GivenAProxyEvent(this->element_fq_id_coherency_, this->event_name_coherency_, CoherencyType::kCoherent)
        .ThatIsSubscribedWithMaxSamples(2U)
        .WithSkeletonEventDataCoherent({{kDummySampleValue, input_timestamp}});

    // and given that GetNewSamples was already called once
    score::cpp::ignore = this->GetNewSamples([](auto, auto) noexcept {}, 37U);

    // and new data older than the previous sample is published
    constexpr EventSlotStatus::EventTimeStamp input_timestamp_2{input_timestamp - 1U};
    score::cpp::ignore = this->PutDataCoherent(kDummySampleValue + 1U, input_timestamp_2);

    // When calling GetNewSamples again
    const auto new_num_samples = this->GetNewSamples(
        [](auto, auto) {
            FAIL() << "Callback was called although no sample was expected.";
        },
        37U);

    // Then no data should be received
    ASSERT_TRUE(new_num_samples.has_value());
    EXPECT_EQ(new_num_samples.value(), 0U);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
