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
#include "score/mw/com/impl/bindings/someip/e2e_event_protection.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>

namespace score::mw::com::impl::someip
{
namespace
{

using e2e::DataIntegrityStatus;
using e2e::SequenceStatus;

constexpr E2EEventTypeDeployment kDeployment{E2EProfile::kP4, 0x42U, 2U};
constexpr std::size_t kPayloadSize{4U};
constexpr SomeIpServiceId kServiceId{0x1234U};
constexpr std::string_view kE2EEventName{"E2EEvent"};
constexpr std::string_view kPlainEventName{"PlainEvent"};
constexpr SomeIpEventId kE2EEventId{1U};
constexpr SomeIpEventId kPlainEventId{2U};

using Frame = std::array<std::byte, kE2EHeaderSize + kPayloadSize>;

Frame MakeFrame(const std::uint8_t payload_value)
{
    Frame frame{};
    for (std::size_t index = kE2EHeaderSize; index < frame.size(); ++index)
    {
        frame[index] = static_cast<std::byte>(payload_value);
    }
    return frame;
}

TEST(SomeIpE2EEventProtectionTest, ProtectedFrameCheckIntegrityAndSequenceOk)
{
    // Given a protector and a checker for the same event
    E2EEventProtector protector{kDeployment};
    E2EEventChecker checker{kDeployment};
    auto frame = MakeFrame(0x11U);

    // When protecting and checking the frame
    ASSERT_TRUE(protector.Protect(frame));
    const auto outcome = checker.Check(frame);

    // Then both checks pass
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kOk);
}

TEST(SomeIpE2EEventProtectionTest, ConsecutiveFramesCheckSequenceOkForEachFrame)
{
    // Given a protector and a checker for the same event
    E2EEventProtector protector{kDeployment};
    E2EEventChecker checker{kDeployment};

    for (std::uint8_t value = 0U; value < 3U; ++value)
    {
        // When protecting and checking frames one after another
        auto frame = MakeFrame(value);
        ASSERT_TRUE(protector.Protect(frame));
        const auto outcome = checker.Check(frame);

        // Then the counter kept by each side advances in step
        EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
        EXPECT_EQ(outcome.sequence, SequenceStatus::kOk);
    }
}

TEST(SomeIpE2EEventProtectionTest, PayloadCorruptedAfterProtectCheckIntegrityErrorAndSequenceSkipped)
{
    // Given a protected frame
    E2EEventProtector protector{kDeployment};
    E2EEventChecker checker{kDeployment};
    auto frame = MakeFrame(0x11U);
    ASSERT_TRUE(protector.Protect(frame));

    // When a payload byte behind the header is changed before checking
    frame.back() = std::byte{0x22U};
    const auto outcome = checker.Check(frame);

    // Then data integrity fails and the sequence check is skipped
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kError);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kSkipped);
}

TEST(SomeIpE2EEventProtectionTest, CheckerWithOtherDataIdCheckIntegrityError)
{
    // Given a frame protected with one data ID
    E2EEventProtector protector{kDeployment};
    E2EEventChecker checker{E2EEventTypeDeployment{E2EProfile::kP4, 0x43U, 2U}};
    auto frame = MakeFrame(0x11U);
    ASSERT_TRUE(protector.Protect(frame));

    // When checking it with another data ID
    const auto outcome = checker.Check(frame);

    // Then data integrity fails
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kError);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kSkipped);
}

TEST(SomeIpE2EEventProtectionTest, HeaderOnlyFrameProtectAndCheckIntegrityOk)
{
    // Given a frame without payload
    E2EEventProtector protector{kDeployment};
    E2EEventChecker checker{kDeployment};
    std::array<std::byte, kE2EHeaderSize> frame{};

    // When protecting and checking it
    ASSERT_TRUE(protector.Protect(frame));
    const auto outcome = checker.Check(frame);

    // Then both checks pass
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kOk);
}

TEST(SomeIpE2EEventProtectionTest, FrameShorterThanHeaderProtectReturnsFalseAndLeavesFrameUntouched)
{
    // Given a frame that cannot hold the header
    E2EEventProtector protector{kDeployment};
    std::array<std::byte, kE2EHeaderSize - 1U> frame{};
    frame.fill(std::byte{0xABU});
    const auto original_frame = frame;

    // When protecting it
    const bool protected_frame = protector.Protect(frame);

    // Then protection fails without writing to the frame
    EXPECT_FALSE(protected_frame);
    EXPECT_EQ(frame, original_frame);
}

TEST(SomeIpE2EEventProtectionTest, FrameShorterThanHeaderCheckIntegrityErrorAndSequenceSkipped)
{
    // Given a received frame that cannot hold the header
    E2EEventChecker checker{kDeployment};
    const std::array<std::byte, kE2EHeaderSize - 1U> frame{};

    // When checking it
    const auto outcome = checker.Check(frame);

    // Then data integrity fails and the sequence check is skipped
    EXPECT_EQ(outcome.data_integrity, DataIntegrityStatus::kError);
    EXPECT_EQ(outcome.sequence, SequenceStatus::kSkipped);
}

TEST(SomeIpE2EEventProtectionTest, EventWithE2EDeploymentCreateReturnsProtectorAndCheckerUsingIt)
{
    // Given a type deployment with E2E configured for one event
    const SomeIpServiceTypeDeployment type_deployment{
        kServiceId, {}, {}, {}, {{std::string{kE2EEventName}, kDeployment}}};

    // When creating the protector and checker for that event
    auto protector = CreateE2EEventProtector(type_deployment, kE2EEventName);
    auto checker = CreateE2EEventChecker(type_deployment, kE2EEventName);

    // Then both are created and accept each other's frames
    ASSERT_TRUE(protector.has_value());
    ASSERT_TRUE(checker.has_value());
    auto frame = MakeFrame(0x11U);
    ASSERT_TRUE(protector->Protect(frame));
    EXPECT_EQ(checker->Check(frame).data_integrity, DataIntegrityStatus::kOk);
}

TEST(SomeIpE2EEventProtectionTest, OnlyOtherEventHasE2EDeploymentCreateReturnsNullopt)
{
    // Given a type deployment with two events, of which only the other one has E2E configured
    const SomeIpServiceTypeDeployment type_deployment{
        kServiceId,
        {{std::string{kE2EEventName}, kE2EEventId}, {std::string{kPlainEventName}, kPlainEventId}},
        {},
        {},
        {{std::string{kE2EEventName}, kDeployment}}};

    // When creating the protector and checker for the event without E2E
    const auto protector = CreateE2EEventProtector(type_deployment, kPlainEventName);
    const auto checker = CreateE2EEventChecker(type_deployment, kPlainEventName);

    // Then neither is created
    EXPECT_FALSE(protector.has_value());
    EXPECT_FALSE(checker.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::someip
