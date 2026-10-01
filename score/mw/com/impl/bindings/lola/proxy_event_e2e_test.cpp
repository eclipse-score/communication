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
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/configuration/e2e_event_type_deployment.h"
#include "score/mw/com/impl/e2e/e2e_profile_stub.h"
#include "score/mw/com/impl/sample_reference_tracker.h"

#include <gtest/gtest.h>

#include <memory>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr E2EEventTypeDeployment kE2EDeployment{E2EProfile::kP4, 0x3AU, 2U};

score::cpp::span<std::byte> AsWritableSpan(e2e::HeaderBytes& header) noexcept
{
    return {header.data(), header.size()};
}

class ProxyEventE2EFixture : public LolaProxyEventResources
{
  protected:
    /// \brief Protects a header for the given value, as SkeletonEventCommon::Send() would, and writes it into the
    ///        shared header storage at the given slot.
    void ProtectDataAtSlot(const SlotIndexType slot_index, const SampleType& value)
    {
        const e2e::ProfileConfiguration profile_config{kE2EDeployment.data_id_, kE2EDeployment.max_delta_counter_};
        const score::cpp::span<const std::byte> payload_span{reinterpret_cast<const std::byte*>(&value),
                                                              sizeof(value)};
        ASSERT_TRUE(e2e::ProtectMessage(
            AsWritableSpan((*header_storage_)[slot_index]), payload_span, profile_config, protect_context_));
    }

    e2e::ProtectContext protect_context_{};
    std::shared_ptr<e2e::HeaderStorage> header_storage_{std::make_shared<e2e::HeaderStorage>(max_num_slots_)};
};

TEST_F(ProxyEventE2EFixture, ValidHeaderProducesOkResultOnDeliveredSample)
{
    // Given a payload value whose slot has a header protected for that exact value
    const SampleType value{4242U};
    const auto slot_index = PutData(value);
    ProtectDataAtSlot(slot_index, value);

    ProxyEvent proxy_event{*proxy_, element_fq_id_, event_name_, kE2EDeployment, header_storage_};
    std::ignore = proxy_event.Subscribe(max_num_slots_);
    SampleReferenceTracker tracker{max_num_slots_};
    TrackerGuardFactory guard_factory{tracker.Allocate(max_num_slots_)};

    // When receiving the sample
    e2e::E2EResult delivered_result{};
    const auto result = proxy_event.GetNewSamples(
        [&proxy_event, &delivered_result](impl::SamplePtr<void>,
                                          const tracing::ITracingRuntime::TracePointDataId) {
            delivered_result = proxy_event.GetLastE2EResult();
        },
        guard_factory);

    // Then the E2E result reports the sample as valid
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1U);
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kOk);
    EXPECT_EQ(delivered_result.sequence, e2e::SequenceStatus::kOk);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kOk);

    proxy_event.Unsubscribe();
}

TEST_F(ProxyEventE2EFixture, TamperedHeaderProducesErrorResultOnDeliveredSample)
{
    // Given a payload value whose slot has a header protected for a *different* value (simulating corruption)
    const SampleType value{4242U};
    const auto slot_index = PutData(value);
    ProtectDataAtSlot(slot_index, value + 1U);

    ProxyEvent proxy_event{*proxy_, element_fq_id_, event_name_, kE2EDeployment, header_storage_};
    std::ignore = proxy_event.Subscribe(max_num_slots_);
    SampleReferenceTracker tracker{max_num_slots_};
    TrackerGuardFactory guard_factory{tracker.Allocate(max_num_slots_)};

    // When receiving the sample
    e2e::E2EResult delivered_result{};
    const auto result = proxy_event.GetNewSamples(
        [&proxy_event, &delivered_result](impl::SamplePtr<void>,
                                          const tracing::ITracingRuntime::TracePointDataId) {
            delivered_result = proxy_event.GetLastE2EResult();
        },
        guard_factory);

    // Then the E2E result reports a data-integrity error
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1U);
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kError);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kError);

    proxy_event.Unsubscribe();
}

}  // namespace
}  // namespace score::mw::com::impl::lola
