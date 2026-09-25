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
#include "score/mw/com/impl/e2e/e2e_profile_stub.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace score::mw::com::impl::e2e
{
namespace
{

constexpr ProfileConfiguration kConfiguration{0x3AU, 2U};
constexpr std::array<std::byte, 3U> kPayload{std::byte{0x01U}, std::byte{0x02U}, std::byte{0x03U}};

using Header = std::array<std::byte, kProfileStubHeaderSize>;

score::cpp::span<std::byte> AsWritableSpan(Header& header) noexcept
{
    return {header.data(), header.size()};
}

score::cpp::span<const std::byte> AsReadOnlySpan(const Header& header) noexcept
{
    return {header.data(), header.size()};
}

score::cpp::span<const std::byte> PayloadSpan() noexcept
{
    return {kPayload.data(), kPayload.size()};
}

TEST(E2eProfileStubTest, ProtectAndCheckValidFrameProducesOkStatuses)
{
    // Given a payload protected with a fresh protect context
    Header header{};
    ProtectContext protect_context{};
    CheckContext check_context{};
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(header), PayloadSpan(), kConfiguration, protect_context));

    // When checking that header for the first time
    const auto outcome = CheckMessage(AsReadOnlySpan(header), PayloadSpan(), kConfiguration, check_context);

    // Then both checks report Ok and the protect counter has advanced
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kOk);
    EXPECT_EQ(protect_context.next_counter, 1U);
}

TEST(E2eProfileStubTest, CheckTamperedHeaderReportsDataIntegrityErrorAndRetainsSequenceState)
{
    // Given a protected header whose checksum byte has been corrupted
    Header header{};
    ProtectContext protect_context{};
    CheckContext check_context{};
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(header), PayloadSpan(), kConfiguration, protect_context));
    header[2U] ^= std::byte{0x01U};

    // When checking the tampered header
    const auto outcome = CheckMessage(AsReadOnlySpan(header), PayloadSpan(), kConfiguration, check_context);

    // Then the data integrity check reports an error and no sequence state is recorded
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kError);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kOk);
    EXPECT_FALSE(check_context.has_last_counter);
}

TEST(E2eProfileStubTest, CheckSameValidFrameTwiceReportsRepeatedCounter)
{
    // Given a valid header that has already been checked once
    Header header{};
    ProtectContext protect_context{};
    CheckContext check_context{};
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(header), PayloadSpan(), kConfiguration, protect_context));
    ASSERT_EQ(CheckMessage(AsReadOnlySpan(header), PayloadSpan(), kConfiguration, check_context).sequence,
              SequenceStatus::kOk);

    // When checking the same header again
    const auto outcome = CheckMessage(AsReadOnlySpan(header), PayloadSpan(), kConfiguration, check_context);

    // Then the sequence check reports a repeated counter
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kErrorRepeated);
}

TEST(E2eProfileStubTest, CheckSkippedCounterWithinConfiguredThresholdReportsGapWithinThreshold)
{
    // Given three consecutive protected frames where only the first and third are checked
    Header first_header{};
    Header ignored_header{};
    Header checked_header{};
    ProtectContext protect_context{};
    CheckContext check_context{};
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(first_header), PayloadSpan(), kConfiguration, protect_context));
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(ignored_header), PayloadSpan(), kConfiguration, protect_context));
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(checked_header), PayloadSpan(), kConfiguration, protect_context));
    ASSERT_EQ(CheckMessage(AsReadOnlySpan(first_header), PayloadSpan(), kConfiguration, check_context).sequence,
              SequenceStatus::kOk);

    // When checking the third frame after the first
    const auto outcome = CheckMessage(AsReadOnlySpan(checked_header), PayloadSpan(), kConfiguration, check_context);

    // Then the sequence check reports a gap within the configured threshold
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kOkGapWithinThreshold);
}

TEST(E2eProfileStubTest, CheckSkippedCounterBeyondConfiguredThresholdReportsGapError)
{
    // Given three consecutive protected frames under a stricter max delta counter, checking only the first and third
    constexpr ProfileConfiguration kStrictConfiguration{0x3AU, 1U};
    Header first_header{};
    Header ignored_header{};
    Header checked_header{};
    ProtectContext protect_context{};
    CheckContext check_context{};
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(first_header), PayloadSpan(), kStrictConfiguration, protect_context));
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(ignored_header), PayloadSpan(), kStrictConfiguration, protect_context));
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(checked_header), PayloadSpan(), kStrictConfiguration, protect_context));
    ASSERT_EQ(CheckMessage(AsReadOnlySpan(first_header), PayloadSpan(), kStrictConfiguration, check_context).sequence,
              SequenceStatus::kOk);

    // When checking the third frame after the first
    const auto outcome =
        CheckMessage(AsReadOnlySpan(checked_header), PayloadSpan(), kStrictConfiguration, check_context);

    // Then the sequence check reports a gap exceeding the threshold
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kErrorGapExceedsThreshold);
}

TEST(E2eProfileStubTest, ProtectRejectsHeaderWithIncorrectSize)
{
    // Given a header buffer smaller than the required POC header size
    std::array<std::byte, kProfileStubHeaderSize - 1U> too_small_header{};
    ProtectContext protect_context{};

    // When protecting a message into that buffer
    const bool result = ProtectMessage(
        {too_small_header.data(), too_small_header.size()}, PayloadSpan(), kConfiguration, protect_context);

    // Then protection fails and the counter is left unchanged
    EXPECT_FALSE(result);
    EXPECT_EQ(protect_context.next_counter, 0U);
}

}  // namespace
}  // namespace score::mw::com::impl::e2e
