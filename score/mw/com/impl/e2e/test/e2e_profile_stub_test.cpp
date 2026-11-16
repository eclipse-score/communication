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

Header ProtectWithCounter(const std::uint8_t counter, const ProfileConfiguration configuration = kConfiguration)
{
    Header header{};
    ProtectContext protect_context{counter};
    EXPECT_TRUE(ProtectMessage(AsWritableSpan(header), PayloadSpan(), configuration, protect_context));
    return header;
}

SequenceStatus CheckValidHeader(const Header& header,
                                CheckContext& check_context,
                                const ProfileConfiguration configuration = kConfiguration)
{
    const auto outcome = CheckMessage(AsReadOnlySpan(header), PayloadSpan(), configuration, check_context);
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    return outcome.sequence;
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

    // Then the data integrity check reports an error, the sequence check is skipped and no sequence state is recorded
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kError);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kSkipped);
    EXPECT_FALSE(check_context.has_last_counter);
}

TEST(E2eProfileStubTest, CheckHeaderWithIncorrectSizeReportsDataIntegrityErrorAndSkippedSequence)
{
    // Given a header buffer smaller than the required POC header size
    std::array<std::byte, kProfileStubHeaderSize - 1U> too_small_header{};
    CheckContext check_context{};

    // When checking that header
    const auto outcome =
        CheckMessage({too_small_header.data(), too_small_header.size()}, PayloadSpan(), kConfiguration, check_context);

    // Then the data integrity check reports an error and the sequence check is skipped
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kError);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kSkipped);
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

TEST(E2eProfileStubTest, CheckCounterWrappingFromMaximumToZeroReportsOk)
{
    // Given two consecutive protected frames where the counter wraps from 255 to 0
    Header header_before_wrap{};
    Header header_after_wrap{};
    ProtectContext protect_context{255U};
    CheckContext check_context{};
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(header_before_wrap), PayloadSpan(), kConfiguration, protect_context));
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(header_after_wrap), PayloadSpan(), kConfiguration, protect_context));
    ASSERT_EQ(CheckMessage(AsReadOnlySpan(header_before_wrap), PayloadSpan(), kConfiguration, check_context).sequence,
              SequenceStatus::kOk);

    // When checking the frame after the wrap
    const auto outcome = CheckMessage(AsReadOnlySpan(header_after_wrap), PayloadSpan(), kConfiguration, check_context);

    // Then the wrap is not reported as a sequence error
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kOk);
}

TEST(E2eProfileStubTest, CheckCounterGoingBackwardsReportsGapError)
{
    // Given two consecutive protected frames checked in reverse order
    Header older_header{};
    Header newer_header{};
    ProtectContext protect_context{};
    CheckContext check_context{};
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(older_header), PayloadSpan(), kConfiguration, protect_context));
    ASSERT_TRUE(ProtectMessage(AsWritableSpan(newer_header), PayloadSpan(), kConfiguration, protect_context));
    ASSERT_EQ(CheckMessage(AsReadOnlySpan(newer_header), PayloadSpan(), kConfiguration, check_context).sequence,
              SequenceStatus::kOk);

    // When checking the older frame after the newer one
    const auto outcome = CheckMessage(AsReadOnlySpan(older_header), PayloadSpan(), kConfiguration, check_context);

    // Then the counter delta wraps to 255 and exceeds the threshold
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kErrorGapExceedsThreshold);
}

TEST(E2eProfileStubTest, CheckForwardGapAboveHalfCounterRangeWithinConfiguredThresholdReportsGapWithinThreshold)
{
    // Given a frame with counter 0 and a frame with counter 200 under the largest max delta counter
    constexpr ProfileConfiguration kLenientConfiguration{0x3AU, 255U};
    Header first_header{};
    Header later_header{};
    ProtectContext first_protect_context{0U};
    ProtectContext later_protect_context{200U};
    CheckContext check_context{};
    ASSERT_TRUE(
        ProtectMessage(AsWritableSpan(first_header), PayloadSpan(), kLenientConfiguration, first_protect_context));
    ASSERT_TRUE(
        ProtectMessage(AsWritableSpan(later_header), PayloadSpan(), kLenientConfiguration, later_protect_context));
    ASSERT_EQ(CheckMessage(AsReadOnlySpan(first_header), PayloadSpan(), kLenientConfiguration, check_context).sequence,
              SequenceStatus::kOk);

    // When checking the frame with counter 200
    const auto outcome =
        CheckMessage(AsReadOnlySpan(later_header), PayloadSpan(), kLenientConfiguration, check_context);

    // Then the gap of 200 is within the configured threshold
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kOkGapWithinThreshold);
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

TEST(E2eProfileStubTest, CheckOutOfOrderFrameReportsGapErrorAndResyncsOnIt)
{
    // Given a checker that has accepted counter 10
    CheckContext check_context{};
    ASSERT_EQ(CheckValidHeader(ProtectWithCounter(10U), check_context), SequenceStatus::kOk);

    // When the older counter 9 and then counter 10 again are received
    const auto outcome_older = CheckValidHeader(ProtectWithCounter(9U), check_context);
    const auto outcome_after = CheckValidHeader(ProtectWithCounter(10U), check_context);

    // Then 9 is reported as a sequence error and the following 10 is in order relative to 9
    EXPECT_EQ(outcome_older, SequenceStatus::kErrorGapExceedsThreshold);
    EXPECT_EQ(outcome_after, SequenceStatus::kOk);
}

TEST(E2eProfileStubTest, CheckAfterSenderRestartRecoversOnNextConsecutiveFrame)
{
    // Given a checker that has accepted counter 200
    CheckContext check_context{};
    ASSERT_EQ(CheckValidHeader(ProtectWithCounter(200U), check_context), SequenceStatus::kOk);

    // When the sender restarts and sends counters 0 and 1
    const auto outcome_restart = CheckValidHeader(ProtectWithCounter(0U), check_context);
    const auto outcome_next = CheckValidHeader(ProtectWithCounter(1U), check_context);

    // Then only the first frame after the restart is reported as a sequence error
    EXPECT_EQ(outcome_restart, SequenceStatus::kErrorGapExceedsThreshold);
    EXPECT_EQ(outcome_next, SequenceStatus::kOk);
}

TEST(E2eProfileStubTest, CheckAfterLossBeyondThresholdRecoversOnNextConsecutiveFrame)
{
    // Given a checker that has accepted counter 0
    CheckContext check_context{};
    ASSERT_EQ(CheckValidHeader(ProtectWithCounter(0U), check_context), SequenceStatus::kOk);

    // When frames 1 to 4 are lost and counters 5 and 6 are received
    const auto outcome_after_loss = CheckValidHeader(ProtectWithCounter(5U), check_context);
    const auto outcome_next = CheckValidHeader(ProtectWithCounter(6U), check_context);

    // Then only the frame after the loss is reported as a sequence error
    EXPECT_EQ(outcome_after_loss, SequenceStatus::kErrorGapExceedsThreshold);
    EXPECT_EQ(outcome_next, SequenceStatus::kOk);
}

TEST(E2eProfileStubTest, CheckRepeatedFrameKeepsReferenceForNextConsecutiveFrame)
{
    // Given a checker that has accepted counter 5
    CheckContext check_context{};
    ASSERT_EQ(CheckValidHeader(ProtectWithCounter(5U), check_context), SequenceStatus::kOk);

    // When counter 5 is received again, followed by counter 6
    const auto outcome_repeated = CheckValidHeader(ProtectWithCounter(5U), check_context);
    const auto outcome_next = CheckValidHeader(ProtectWithCounter(6U), check_context);

    // Then the repetition is reported and 6 is in order
    EXPECT_EQ(outcome_repeated, SequenceStatus::kErrorRepeated);
    EXPECT_EQ(outcome_next, SequenceStatus::kOk);
}

TEST(E2eProfileStubTest, CheckCorruptedFrameDoesNotMoveSequenceReference)
{
    // Given a checker that has accepted counter 0
    CheckContext check_context{};
    ASSERT_EQ(CheckValidHeader(ProtectWithCounter(0U), check_context), SequenceStatus::kOk);

    // When a frame with counter 7 and a corrupted checksum is received, followed by a valid counter 1
    auto corrupted_header = ProtectWithCounter(7U);
    corrupted_header[2U] ^= std::byte{0x01U};
    const auto outcome_corrupted =
        CheckMessage(AsReadOnlySpan(corrupted_header), PayloadSpan(), kConfiguration, check_context);
    const auto outcome_next = CheckValidHeader(ProtectWithCounter(1U), check_context);

    // Then the corrupted frame is rejected without affecting the sequence check of counter 1
    EXPECT_EQ(outcome_corrupted.data_integrity, DataIntegrityStatus::kError);
    EXPECT_EQ(outcome_corrupted.sequence, SequenceStatus::kSkipped);
    EXPECT_EQ(outcome_next, SequenceStatus::kOk);
}

TEST(E2eProfileStubTest, CheckGapAcrossCounterWrapWithinThresholdReportsGapWithinThreshold)
{
    // Given a checker that has accepted counter 255
    CheckContext check_context{};
    ASSERT_EQ(CheckValidHeader(ProtectWithCounter(255U), check_context), SequenceStatus::kOk);

    // When counter 1 is received after counter 0 was lost
    const auto outcome = CheckValidHeader(ProtectWithCounter(1U), check_context);

    // Then the gap of 2 across the wrap is within the threshold
    EXPECT_EQ(outcome, SequenceStatus::kOkGapWithinThreshold);
}

TEST(E2eProfileStubTest, CheckCounterGoingBackOneWithMaximumThresholdIsNotDetected)
{
    // Given a checker with the largest max delta counter that has accepted counter 10
    constexpr ProfileConfiguration kLenientConfiguration{0x3AU, 255U};
    CheckContext check_context{};
    ASSERT_EQ(CheckValidHeader(ProtectWithCounter(10U, kLenientConfiguration), check_context, kLenientConfiguration),
              SequenceStatus::kOk);

    // When the older counter 9 is received
    const auto outcome =
        CheckValidHeader(ProtectWithCounter(9U, kLenientConfiguration), check_context, kLenientConfiguration);

    // Then a step back by one is indistinguishable from a forward gap of 255
    EXPECT_EQ(outcome, SequenceStatus::kOkGapWithinThreshold);
}

TEST(E2eProfileStubTest, CheckFrameWithOtherDataIdReportsDataIntegrityErrorAndRetainsSequenceState)
{
    // Given a frame protected with another data ID
    constexpr ProfileConfiguration kOtherDataIdConfiguration{0x3BU, 2U};
    const auto header = ProtectWithCounter(0U, kOtherDataIdConfiguration);
    CheckContext check_context{};

    // When checking it
    const auto outcome = CheckMessage(AsReadOnlySpan(header), PayloadSpan(), kConfiguration, check_context);

    // Then the data integrity check reports an error and no sequence state is recorded
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kError);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kSkipped);
    EXPECT_FALSE(check_context.has_last_counter);
}

TEST(E2eProfileStubTest, CheckFrameWithDataIdDifferingOnlyInUpperBytesReportsDataIntegrityError)
{
    // Given a frame protected with a data ID that has the same low byte as the configured one
    constexpr ProfileConfiguration kUpperBytesConfiguration{0x0100003AU, 2U};
    const auto header = ProtectWithCounter(0U, kUpperBytesConfiguration);
    CheckContext check_context{};

    // When checking it
    const auto outcome = CheckMessage(AsReadOnlySpan(header), PayloadSpan(), kConfiguration, check_context);

    // Then the full 32-bit data ID is covered by the checksum and the frame is rejected
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kError);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kSkipped);
}

}  // namespace
}  // namespace score::mw::com::impl::e2e
