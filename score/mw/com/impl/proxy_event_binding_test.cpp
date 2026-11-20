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
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/e2e/e2e_result.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/sample_reference_tracker.h"

#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

class DummyProxyEventBinding final : public ProxyEventBinding
{
  public:
    using ProxyEventBinding::MakeSamplePtr;

    Result<void> Subscribe(std::size_t) noexcept override
    {
        return {};
    }
    SubscriptionState GetSubscriptionState() const noexcept override
    {
        return SubscriptionState::kSubscribed;
    }
    void Unsubscribe() noexcept override {}
    Result<void> SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler>) noexcept override
    {
        return {};
    }
    Result<void> UnsetReceiveHandler() noexcept override
    {
        return {};
    }
    Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler) noexcept override
    {
        return {};
    }
    Result<void> UnsetSubscriptionStateChangeHandler() noexcept override
    {
        return {};
    }
    Result<std::size_t> GetNumNewSamplesAvailable() const override
    {
        return {};
    }
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept override
    {
        return {};
    }
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kFake;
    }

    Result<std::size_t> GetNewSamples(Callback&&, TrackerGuardFactory&) noexcept override
    {
        return {};
    }
};

TEST(ProxyEventBindingTest, ProxyEventBindingBaseShouldNotBeCopyable)
{
    static_assert(!std::is_copy_constructible<DummyProxyEventBinding>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<DummyProxyEventBinding>::value, "Is wrongly copyable");
}

TEST(ProxyEventBindingTest, ProxyEventBindingBaseShouldNotBeMoveable)
{
    static_assert(!std::is_move_constructible<DummyProxyEventBinding>::value, "Is wrongly moveable");
    static_assert(!std::is_move_assignable<DummyProxyEventBinding>::value, "Is wrongly moveable");
}

TEST(ProxyEventBindingMakeSamplePtrTest, WrapsBindingSamplePointer)
{
    // Given a binding-specific sample pointer
    auto* const sample = new int{42};
    auto binding_ptr = mock_binding::MakeSamplePtr(sample);

    // When creating a binding-independent SamplePtr from it
    const auto unit = DummyProxyEventBinding::MakeSamplePtr(std::move(binding_ptr), SampleReferenceGuard{});

    // Then the SamplePtr is valid and references the same sample
    ASSERT_TRUE(unit);
    EXPECT_EQ(unit.get(), sample);
}

TEST(ProxyEventBindingMakeSamplePtrTest, DefaultRawCheckOutcomeLeavesE2EResultDisabled)
{
    // Given a binding-specific sample pointer
    auto binding_ptr = mock_binding::MakeSamplePtr(new int{42});

    // When creating a SamplePtr without providing a RawCheckOutcome
    const auto unit = DummyProxyEventBinding::MakeSamplePtr(std::move(binding_ptr), SampleReferenceGuard{});

    // Then all E2E result fields are kDisabled
    const auto e2e_result = unit.GetE2EResult();
    EXPECT_EQ(e2e_result.data_integrity, e2e::DataIntegrityStatus::kDisabled);
    EXPECT_EQ(e2e_result.sequence, e2e::SequenceStatus::kDisabled);
    EXPECT_EQ(e2e_result.historical_health, e2e::HistoricalHealthStatus::kDisabled);
    EXPECT_EQ(e2e_result.summary, e2e::Summary::kDisabled);
}

TEST(ProxyEventBindingMakeSamplePtrTest, ProvidedRawCheckOutcomeIsCarriedOnSamplePtr)
{
    // Given a binding-specific sample pointer and a failing RawCheckOutcome from the binding's E2E check
    auto binding_ptr = mock_binding::MakeSamplePtr(new int{42});
    const e2e::RawCheckOutcome raw_check_outcome{e2e::DataIntegrityStatus::kError,
                                                 e2e::SequenceStatus::kErrorGapExceedsThreshold};

    // When creating a SamplePtr with that RawCheckOutcome
    const auto unit =
        DummyProxyEventBinding::MakeSamplePtr(std::move(binding_ptr), SampleReferenceGuard{}, raw_check_outcome);

    // Then data integrity and sequence are taken over from the binding
    const auto e2e_result = unit.GetE2EResult();
    EXPECT_EQ(e2e_result.data_integrity, e2e::DataIntegrityStatus::kError);
    EXPECT_EQ(e2e_result.sequence, e2e::SequenceStatus::kErrorGapExceedsThreshold);

    // And historical health and summary remain kDisabled, as only the binding-independent layer may set them
    EXPECT_EQ(e2e_result.historical_health, e2e::HistoricalHealthStatus::kDisabled);
    EXPECT_EQ(e2e_result.summary, e2e::Summary::kDisabled);
}

TEST(ProxyEventBindingMakeSamplePtrTest, TakesOwnershipOfReferenceGuard)
{
    // Given a SampleReferenceTracker from which one guard was taken
    constexpr std::size_t max_num_samples{2U};
    SampleReferenceTracker tracker{max_num_samples};
    {
        auto guard_factory = tracker.Allocate(1U);
        auto guard = guard_factory.TakeGuard();
        ASSERT_TRUE(guard.has_value());

        // When creating a SamplePtr with that guard
        auto binding_ptr = mock_binding::MakeSamplePtr(new int{42});
        const auto unit = DummyProxyEventBinding::MakeSamplePtr(std::move(binding_ptr), std::move(guard).value());

        // Then the sample stays accounted for as in use while the SamplePtr is alive
        EXPECT_EQ(tracker.GetNumAvailableSamples(), max_num_samples - 1U);
    }

    // And the sample is released back to the tracker once the SamplePtr is destroyed
    EXPECT_EQ(tracker.GetNumAvailableSamples(), max_num_samples);
}

}  // namespace
}  // namespace score::mw::com::impl
