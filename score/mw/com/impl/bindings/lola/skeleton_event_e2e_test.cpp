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
#include "score/mw/com/impl/bindings/lola/test/skeleton_event_test_resources.h"
#include "score/mw/com/impl/configuration/e2e_event_type_deployment.h"
#include "score/mw/com/impl/e2e/e2e_profile_stub.h"

#include <gtest/gtest.h>

#include <memory>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr E2EEventTypeDeployment kE2EDeployment{E2EProfile::kP4, 0x3AU, 2U};

score::cpp::span<const std::byte> AsReadOnlySpan(const e2e::HeaderBytes& header) noexcept
{
    return {header.data(), header.size()};
}

class SkeletonEventE2EFixture : public SkeletonEventFixture
{
  protected:
    std::shared_ptr<e2e::HeaderStorage> header_storage_{std::make_shared<e2e::HeaderStorage>(max_samples_)};
};

TEST_F(SkeletonEventE2EFixture, SendWritesHeaderThatChecksOutForTheSentValue)
{
    // Given a skeleton event configured with a POC E2E profile and a shared (injected) header storage
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            true,
                            QualityType::kASIL_QM,
                            {},
                            false,
                            kE2EDeployment,
                            header_storage_);
    std::ignore = skeleton_event_->PrepareOffer(std::nullopt);

    // When sending a value
    const test::TestSampleType sent_value{42U};
    ASSERT_TRUE(SendValueByCopy(*skeleton_event_, sent_value).has_value());

    // Then the header written for that slot checks out for the same payload bytes
    e2e::CheckContext check_context{};
    const e2e::ProfileConfiguration profile_config{kE2EDeployment.data_id_, kE2EDeployment.max_delta_counter_};
    const score::cpp::span<const std::byte> payload_span{reinterpret_cast<const std::byte*>(&sent_value),
                                                          sizeof(sent_value)};

    bool found_valid_header{false};
    for (const auto& header : *header_storage_)
    {
        const auto outcome = e2e::CheckMessage(AsReadOnlySpan(header), payload_span, profile_config, check_context);
        if (outcome.data_integrity == e2e::DataIntegrityStatus::kOk)
        {
            found_valid_header = true;
            break;
        }
    }
    EXPECT_TRUE(found_valid_header);
}

TEST_F(SkeletonEventE2EFixture, SendWithoutE2EConfigurationLeavesHeaderStorageUntouched)
{
    // Given a skeleton event with a header storage injected but no configured E2E profile
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            true,
                            QualityType::kASIL_QM,
                            {},
                            false,
                            std::nullopt,
                            header_storage_);
    std::ignore = skeleton_event_->PrepareOffer(std::nullopt);

    // When sending a value
    const test::TestSampleType sent_value{99U};
    ASSERT_TRUE(SendValueByCopy(*skeleton_event_, sent_value).has_value());

    // Then no header was ever written (all-zero, since E2E is not configured for this event)
    for (const auto& header : *header_storage_)
    {
        for (const auto header_byte : header)
        {
            EXPECT_EQ(header_byte, std::byte{0U});
        }
    }
}

}  // namespace
}  // namespace score::mw::com::impl::lola
