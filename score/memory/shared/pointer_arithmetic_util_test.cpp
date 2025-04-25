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
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>
#include <utility>

namespace score::memory::shared::test
{

namespace
{

constexpr auto kInt32Min = std::numeric_limits<std::int32_t>::min();
constexpr auto kInt32Max = std::numeric_limits<std::int32_t>::max();
constexpr auto kUInt32Max = std::numeric_limits<std::uint32_t>::max();

}  // namespace

const auto kPtrDiffTMax = static_cast<std::uintptr_t>(std::numeric_limits<std::ptrdiff_t>::max());

class PointerArithmeticSubtractValidPointerBytesFixture
    : public ::testing::TestWithParam<std::tuple<const void*, const void*, std::ptrdiff_t>>
{
};

TEST_P(PointerArithmeticSubtractValidPointerBytesFixture,
       SubtractingPointersLessThanMaxDistanceAwayReturnsDifferenceInBytes)
{
    const auto [first_address, second_address, expected_difference_in_bytes] = GetParam();

    // When subtracting two pointers whose absolute difference is smaller than
    // kPtrDiffTMax
    const auto actual_difference_in_bytes = SubtractPointersBytes(first_address, second_address);

    // Then the result will be the number of bytes between them
    EXPECT_EQ(expected_difference_in_bytes, actual_difference_in_bytes);
}

INSTANTIATE_TEST_SUITE_P(
    PointerArithmeticSubtractValidPointerBytesFixture,
    PointerArithmeticSubtractValidPointerBytesFixture,
    ::testing::Values(
        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{0}),
                        reinterpret_cast<const void*>(std::uintptr_t{0}),
                        0),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{1}),
                        reinterpret_cast<const void*>(std::uintptr_t{1}),
                        0),

        std::make_tuple(reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max()),
                        reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max()),
                        0),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{10}),
                        reinterpret_cast<const void*>(std::uintptr_t{0}),
                        10),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{0}),
                        reinterpret_cast<const void*>(std::uintptr_t{10}),
                        -10),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{5}),
                        reinterpret_cast<const void*>(std::uintptr_t{10}),
                        -5),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{10}),
                        reinterpret_cast<const void*>(std::uintptr_t{5}),
                        5),

        std::make_tuple(reinterpret_cast<const void*>(kPtrDiffTMax),
                        reinterpret_cast<const void*>(std::uintptr_t{0}),
                        kPtrDiffTMax),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{0}),
                        reinterpret_cast<const void*>(kPtrDiffTMax),
                        -kPtrDiffTMax),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{0}),
                        reinterpret_cast<const void*>(kPtrDiffTMax + 1U),
                        std::numeric_limits<std::ptrdiff_t>::min()),

        std::make_tuple(reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max() - kPtrDiffTMax),
                        reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max()),
                        -kPtrDiffTMax),

        std::make_tuple(reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max()),
                        reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max() - kPtrDiffTMax),
                        kPtrDiffTMax)));

class PointerArithmeticSubtractOverflowPointerBytesFixture
    : public ::testing::TestWithParam<std::pair<const void*, const void*>>
{
};

TEST_P(PointerArithmeticSubtractOverflowPointerBytesFixture, SubtractingPointersMoreThanMaxDistanceAwayTerminates)
{
    const auto [first_address, second_address] = GetParam();

    // When subtracting two pointers whose absolute difference is larger than
    // kPtrDiffTMax
    // Then the program will terminate
    // TODO: Replace with SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED when SubtractPointersBytes is no longer noexcept
    EXPECT_DEATH(SubtractPointersBytes(first_address, second_address), ".*");
}

// We make assumptions in this test about the size of std::uintptr_t and std::ptrdiff_t based on the system we're using
// (i.e. that they contain the same number of bytes but the former is unsigned while the latter is signed). However, a
// different system could use different sizes for these types. In these cases, these tests may fail which is desired so
// that we're made aware of these system differences.
INSTANTIATE_TEST_SUITE_P(
    PointerArithmeticSubtractOverflowPointerBytesFixture,
    PointerArithmeticSubtractOverflowPointerBytesFixture,
    ::testing::Values(std::make_pair(reinterpret_cast<const void*>(std::uintptr_t{
                                         std::numeric_limits<std::uintptr_t>::max()}),
                                     reinterpret_cast<const void*>(std::uintptr_t{0})),

                      std::make_pair(reinterpret_cast<const void*>(std::uintptr_t{0}),
                                     reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max())),

                      std::make_pair(reinterpret_cast<const void*>(kPtrDiffTMax + 1U),
                                     reinterpret_cast<const void*>(std::uintptr_t{0})),

                      std::make_pair(reinterpret_cast<const void*>(kPtrDiffTMax + 1U),
                                     reinterpret_cast<const void*>(std::uintptr_t{0})),

                      std::make_pair(reinterpret_cast<const void*>(kPtrDiffTMax + 21U),
                                     reinterpret_cast<const void*>(std::uintptr_t{20})),

                      std::make_pair(reinterpret_cast<const void*>(std::uintptr_t{20}),
                                     reinterpret_cast<const void*>(kPtrDiffTMax + 22U))));

class PointerArithmeticAddOffsetToPointerFixture
    : public ::testing::TestWithParam<std::tuple<const void*, std::size_t, const void*>>
{
};

TEST_P(PointerArithmeticAddOffsetToPointerFixture,
       AddingOffsetToPointerWhichWouldNotCauseOverFlowShouldReturnValidAddress)
{
    const auto [pointer, offset, expected_pointer_result] = GetParam();

    // When adding an offset to a pointer which would not lead to an overflow
    auto* const actual_pointer_result = AddOffsetToPointer(pointer, offset);

    // Then the result will be a pointer offset bytes away from the base pointer
    EXPECT_EQ(expected_pointer_result, actual_pointer_result);
}

INSTANTIATE_TEST_SUITE_P(
    PointerArithmeticAddOffsetToPointerFixture,
    PointerArithmeticAddOffsetToPointerFixture,
    ::testing::Values(
        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{0}),
                        std::size_t{0},
                        reinterpret_cast<const void*>(std::uintptr_t{0})),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{10}),
                        std::size_t{20},
                        reinterpret_cast<const void*>(std::uintptr_t{30})),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::uintptr_t>::max()}),
                        std::size_t{0},
                        reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::uintptr_t>::max()})),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{0}),
                        std::numeric_limits<std::size_t>::max(),
                        reinterpret_cast<void*>(static_cast<std::uintptr_t>(std::numeric_limits<std::size_t>::max()))),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::uintptr_t>::max() - 10}),
                        std::size_t{10},
                        reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::uintptr_t>::max()})),

        std::make_tuple(
            reinterpret_cast<const void*>(std::uintptr_t{10}),
            std::numeric_limits<std::size_t>::max() - 10,
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(std::numeric_limits<std::size_t>::max())))));

class PointerArithmeticAddOffsetToPointerOverflowFixture
    : public ::testing::TestWithParam<std::pair<const void*, std::size_t>>
{
};

TEST_P(PointerArithmeticAddOffsetToPointerOverflowFixture,
       AddingOffsetToPointerWhichWouldNotCauseOverFlowShouldReturnValidAddress)
{
    const auto [pointer, offset] = GetParam();

    // When adding an offset to a pointer which would lead to an overflow
    // Then the program will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(AddOffsetToPointer(pointer, offset));
}

INSTANTIATE_TEST_SUITE_P(
    PointerArithmeticAddOffsetToPointerOverflowFixture,
    PointerArithmeticAddOffsetToPointerOverflowFixture,
    ::testing::Values(
        std::make_pair(reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max()), std::size_t{1}),

        std::make_pair(reinterpret_cast<const void*>(std::uintptr_t{1}), std::numeric_limits<std::size_t>::max())));

TEST(PointerArithmeticAddOffsetToPointerTest, AddOffsetToPointerSupportsPointerToNonConst)
{
    // Given a pointer to const void and an offset
    auto* const pointer = reinterpret_cast<void*>(std::uintptr_t{10});
    const std::size_t offset{20U};

    // When adding the offset to the pointer which would not lead to an overflow
    auto* const actual_pointer_result = AddOffsetToPointer(pointer, offset);

    // Then the result will be a pointer offset bytes away from the base pointer
    auto* const expected_pointer_result = reinterpret_cast<void*>(std::uintptr_t{30});
    EXPECT_EQ(expected_pointer_result, actual_pointer_result);
}

class PointerArithmeticAddSignedOffsetToPointerFixture
    : public ::testing::TestWithParam<std::tuple<const void*, std::ptrdiff_t, const void*>>
{
};

TEST_P(PointerArithmeticAddSignedOffsetToPointerFixture,
       AddingOffsetToPointerWhichWouldNotCauseOverFlowShouldReturnValidAddress)
{
    const auto [pointer, offset, expected_pointer_result] = GetParam();

    // When adding an offset to a pointer which would not lead to an overflow
    auto* const actual_pointer_result = AddOffsetToPointer(pointer, offset);

    // Then the result will be a pointer offset bytes away from the base pointer
    EXPECT_EQ(expected_pointer_result, actual_pointer_result);
}

// We make assumptions in this test about the size of std::uintptr_t and std::ptrdiff_t based on the system we're using
// (i.e. that they contain the same number of bytes but the former is unsigned while the latter is signed). However, a
// different system could use different sizes for these types. In these cases, these tests may fail which is desired so
// that we're made aware of these system differences.
INSTANTIATE_TEST_SUITE_P(
    PointerArithmeticAddSignedOffsetToPointerFixture,
    PointerArithmeticAddSignedOffsetToPointerFixture,
    ::testing::Values(
        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{0}),
                        std::ptrdiff_t{0},
                        reinterpret_cast<const void*>(std::uintptr_t{0})),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{10}),
                        std::ptrdiff_t{20},
                        reinterpret_cast<const void*>(std::uintptr_t{30})),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{20}),
                        std::ptrdiff_t{-10},
                        reinterpret_cast<const void*>(std::uintptr_t{10})),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::uintptr_t>::max()}),
                        std::ptrdiff_t{0},
                        reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::uintptr_t>::max()})),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::ptrdiff_t>::max()} + 1U),
                        std::numeric_limits<std::ptrdiff_t>::min(),
                        reinterpret_cast<const void*>(std::uintptr_t{0U})),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::uintptr_t>::max()}),
                        std::numeric_limits<std::ptrdiff_t>::min(),
                        reinterpret_cast<const void*>(
                            std::numeric_limits<std::uintptr_t>::max() -
                            (static_cast<std::uintptr_t>(std::numeric_limits<std::ptrdiff_t>::max()) + 1U))),

        std::make_tuple(
            reinterpret_cast<const void*>(std::uintptr_t{0}),
            std::numeric_limits<std::ptrdiff_t>::max(),
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(std::numeric_limits<std::ptrdiff_t>::max()))),

        std::make_tuple(reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::uintptr_t>::max() - 10}),
                        std::ptrdiff_t{10},
                        reinterpret_cast<const void*>(std::uintptr_t{std::numeric_limits<std::uintptr_t>::max()})),

        std::make_tuple(
            reinterpret_cast<const void*>(std::uintptr_t{10}),
            std::numeric_limits<std::ptrdiff_t>::max() - 10,
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(std::numeric_limits<std::ptrdiff_t>::max())))

            ));

class PointerArithmeticAddSignedOffsetToPointerOverflowFixture
    : public ::testing::TestWithParam<std::pair<const void*, std::ptrdiff_t>>
{
};

TEST_P(PointerArithmeticAddSignedOffsetToPointerOverflowFixture,
       AddingOffsetToPointerWhichWouldNotCauseOverFlowShouldReturnValidAddress)
{
    const auto [pointer, offset] = GetParam();

    // When adding an offset to a pointer which would lead to an overflow
    // Then the program will terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(AddOffsetToPointer(pointer, offset));
}

// We make assumptions in this test about the size of std::uintptr_t and std::ptrdiff_t based on the system we're using
// (i.e. that they contain the same number of bytes but the former is unsigned while the latter is signed). However, a
// different system could use different sizes for these types. In these cases, these tests may fail which is desired so
// that we're made aware of these system differences.
INSTANTIATE_TEST_SUITE_P(
    PointerArithmeticAddSignedOffsetToPointerOverflowFixture,
    PointerArithmeticAddSignedOffsetToPointerOverflowFixture,
    ::testing::Values(std::make_pair(reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max()),
                                     std::ptrdiff_t{1}),

                      std::make_pair(reinterpret_cast<const void*>(std::uintptr_t{0U}), std::ptrdiff_t{-1}),

                      std::make_pair(reinterpret_cast<const void*>(std::uintptr_t{10U}), std::ptrdiff_t{-11}),

                      std::make_pair(reinterpret_cast<const void*>(
                                         static_cast<std::uintptr_t>(std::numeric_limits<std::ptrdiff_t>::max())),
                                     std::numeric_limits<std::ptrdiff_t>::min())));

TEST(PointerArithmeticAddSignedOffsetToPointerTest, AddSignedOffsetToPointerSupportsPointerToNonConst)
{
    // Given a pointer to const void and an offset
    auto* const pointer = reinterpret_cast<void*>(std::uintptr_t{30});
    const std::ptrdiff_t offset{-10};

    // When adding the offset to the pointer which would not lead to an overflow
    auto* const actual_pointer_result = AddOffsetToPointer(pointer, offset);

    // Then the result will be a pointer offset bytes away from the base pointer
    auto* const expected_pointer_result = reinterpret_cast<void*>(std::uintptr_t{20});
    EXPECT_EQ(expected_pointer_result, actual_pointer_result);
}

class PointerArithmeticCastPointerFixture : public ::testing::TestWithParam<std::pair<const void*, std::uintptr_t>>
{
};

TEST_P(PointerArithmeticCastPointerFixture, CastingPointerToIntegerReturnsAddressAsInteger)
{
    const auto [pointer, expected_integer_address] = GetParam();

    // When casting a pointer to an integer
    const auto actual_integer_address = CastPointerToInteger(pointer);

    // Then the resulting integer will be the same pointer address represented as an integer
    EXPECT_EQ(expected_integer_address, actual_integer_address);
}

TEST_P(PointerArithmeticCastPointerFixture, CastingIntegerToAddressReturnsAddressAsPointer)
{
    const auto [expected_pointer, integer_address] = GetParam();

    // When casting an integer to a pointer
    const auto* const actual_pointer = CastIntegerToPointer<const void*>(integer_address);

    // Then the pointer address will be the same as the integer value
    EXPECT_EQ(expected_pointer, actual_pointer);
}

INSTANTIATE_TEST_SUITE_P(
    PointerArithmeticCastPointerFixture,
    PointerArithmeticCastPointerFixture,
    ::testing::Values(std::make_pair(reinterpret_cast<const void*>(std::uintptr_t{0}), std::uintptr_t{0}),

                      std::make_pair(reinterpret_cast<const void*>(std::uintptr_t{100}), std::uintptr_t{100}),

                      std::make_pair(reinterpret_cast<const void*>(std::uintptr_t{
                                         std::numeric_limits<std::uintptr_t>::max()}),
                                     std::uintptr_t{std::numeric_limits<std::uintptr_t>::max()})

                          ));

TEST(PointerArithmeticCastPointerTest, CastingIntegerToPointerSupportsPointerToNonConst)
{
    const std::uintptr_t integer_address{100};

    // When casting an integer to a pointer
    const auto* const actual_pointer = CastIntegerToPointer<const void*>(integer_address);

    // Then the pointer address will be the same as the integer value
    auto* const expected_pointer = reinterpret_cast<void*>(integer_address);
    EXPECT_EQ(expected_pointer, actual_pointer);
}

class PointerArithmeticAbsoluteValueIntFixture : public ::testing::TestWithParam<std::pair<std::int32_t, std::uint32_t>>
{
};

TEST_P(PointerArithmeticAbsoluteValueIntFixture,
       WhenCalculatingAbsoluteValueOfASignedTypeReturnsTheCorrectValueAsUnsignedType)
{
    const auto [signed_value, expected_absolute_value] = GetParam();

    // When calculating the absolute value of a signed value
    const auto actual_absolute_value = AbsoluteValue(signed_value);

    // Then the calculated absolute value should match the expected value
    EXPECT_EQ(actual_absolute_value, expected_absolute_value);
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticAbsoluteValueIntFixture,
                         PointerArithmeticAbsoluteValueIntFixture,
                         ::testing::Values(std::make_pair(0, 0U),
                                           std::make_pair(1, 1U),
                                           std::make_pair(-1, 1U),
                                           std::make_pair(-100, 100U),
                                           std::make_pair(kInt32Min, kInt32Max + 1U),
                                           std::make_pair(kInt32Max, kInt32Max)));

class PointerArithmeticAbsoluteValueInt8Fixture : public ::testing::TestWithParam<std::pair<std::int8_t, std::uint8_t>>
{
};

TEST_P(PointerArithmeticAbsoluteValueInt8Fixture,
       WhenCalculatingAbsoluteValueOfASignedTypeReturnsTheCorrectValueAsUnsignedType)
{
    const auto [signed_value, expected_absolute_value] = GetParam();

    // When calculating the absolute value of a signed 8 bit int value
    const auto actual_absolute_value = AbsoluteValue(signed_value);

    // Then the calculated absolute value should match the expected value
    EXPECT_EQ(actual_absolute_value, expected_absolute_value);
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticAbsoluteValueInt8Fixture,
                         PointerArithmeticAbsoluteValueInt8Fixture,
                         ::testing::Values(std::make_pair(0, 0U),
                                           std::make_pair(1, 1U),
                                           std::make_pair(-1, 1U),
                                           std::make_pair(-100, 100U),
                                           std::make_pair(-128, 128),
                                           std::make_pair(127, 127)));

class PointerArithmeticUndoSignedIntToUnsignedIntCastFixture : public ::testing::TestWithParam<std::int32_t>
{
};

TEST_P(PointerArithmeticUndoSignedIntToUnsignedIntCastFixture,
       WhenUndoingSignedToUnsignedIntegerCastReturnsTheCorrectValueAsSignedType)
{
    const auto signed_value = GetParam();

    // Given a signed integer that was created by casting an unsigned integer to a signed integer (which is defined by
    // the standard)
    const auto unsigned_value = static_cast<std::uint32_t>(signed_value);

    // When casting the unsigned integer back to a signed integer
    const auto signed_value_result = UndoSignedToUnsignedIntegerCast(unsigned_value);

    // Then the resulting signed integer should be the same as the original integer
    EXPECT_EQ(signed_value_result, signed_value);
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticUndoSignedIntToUnsignedIntCastFixture,
                         PointerArithmeticUndoSignedIntToUnsignedIntCastFixture,
                         ::testing::Values(0, -1, 1, kInt32Min, kInt32Min + 1, kInt32Max, kInt32Max - 1));

class PointerArithmeticUndoSignedInt8ToUnsignedInt8CastFixture : public ::testing::TestWithParam<std::int8_t>
{
};

TEST_P(PointerArithmeticUndoSignedInt8ToUnsignedInt8CastFixture,
       WhenUndoingSignedToUnsignedIntegerCastReturnsTheCorrectValueAsSignedType)
{
    const auto signed_value = GetParam();

    // Given a signed 8 bit integer that was created by casting an unsigned 8 bit integer to a signed 8 bit integer
    // (which is defined by the standard)
    const auto unsigned_value = static_cast<std::uint8_t>(signed_value);

    // When casting the unsigned 8 bit integer back to a 8 bit signed integer
    const auto signed_value_result = UndoSignedToUnsignedIntegerCast(unsigned_value);

    // Then the resulting signed 8 bit integer should be the same as the original 8 bit integer
    EXPECT_EQ(signed_value_result, signed_value);
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticUndoSignedInt8ToUnsignedInt8CastFixture,
                         PointerArithmeticUndoSignedInt8ToUnsignedInt8CastFixture,
                         ::testing::Values(0, -1, 1, -128, -127, 127, 126));

class PointerArithmeticAddSignedIntAndUnsignedIntFixture
    : public ::testing::TestWithParam<std::tuple<std::int32_t, std::uint32_t, std::int32_t>>
{
};

TEST_P(PointerArithmeticAddSignedIntAndUnsignedIntFixture,
       WhenAddingUnsignedIntToSignedIntReturnsCorrectValueAsSignedInt)
{
    const auto [signed_value, unsigned_value, expected_result] = GetParam();

    // When adding an unsigned integer to a signed 8 bit integer
    const auto actual_result = AddUnsignedToSigned(signed_value, unsigned_value);

    // Then the resulting signed 8 bit integer should equal the expected value
    EXPECT_EQ(actual_result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticAddSignedIntAndUnsignedIntFixture,
                         PointerArithmeticAddSignedIntAndUnsignedIntFixture,
                         ::testing::Values(std::make_tuple(0, 0U, 0),
                                           std::make_tuple(0, 1U, 1),
                                           std::make_tuple(1, 0U, 1),

                                           std::make_tuple(10, 20U, 30),
                                           std::make_tuple(0, kInt32Max, kInt32Max),
                                           std::make_tuple(10, kInt32Max - 10U, kInt32Max),

                                           std::make_tuple(-1, 0U, -1),
                                           std::make_tuple(-10, 10U, 0),
                                           std::make_tuple(-10, 9U, -1),
                                           std::make_tuple(-10, 11U, 1),

                                           std::make_tuple(kInt32Min, kInt32Max, -1),
                                           std::make_tuple(kInt32Min, kUInt32Max, kInt32Max)));

class PointerArithmeticAddSignedInt8AndUnsignedInt8Fixture
    : public ::testing::TestWithParam<std::tuple<std::int8_t, std::uint8_t, std::int8_t>>
{
};

TEST_P(PointerArithmeticAddSignedInt8AndUnsignedInt8Fixture,
       WhenAddingUnsignedInt8ToSignedInt8ReturnsCorrectValueAsSignedInt8)
{
    const auto [signed_value, unsigned_value, expected_result] = GetParam();

    // When adding an unsigned 8 bit integer to a signed 8 bit integer
    const auto actual_result = AddUnsignedToSigned(signed_value, unsigned_value);

    // Then the resulting signed 8 bit integer should equal the expected value
    EXPECT_EQ(actual_result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticAddSignedInt8AndUnsignedInt8Fixture,
                         PointerArithmeticAddSignedInt8AndUnsignedInt8Fixture,
                         ::testing::Values(std::make_tuple(0, 0U, 0),
                                           std::make_tuple(0, 1U, 1),
                                           std::make_tuple(1, 0U, 1),

                                           std::make_tuple(10, 20U, 30),
                                           std::make_tuple(0, 127, 127),
                                           std::make_tuple(10, 117, 127),

                                           std::make_tuple(-1, 0U, -1),
                                           std::make_tuple(-10, 10U, 0),
                                           std::make_tuple(-10, 9U, -1),
                                           std::make_tuple(-10, 11U, 1),

                                           std::make_tuple(-128, 127U, -1),
                                           std::make_tuple(-128, 255U, 127U)));

class PointerArithmeticAddSignedIntAndUnsignedIntPreconditionViolationFixture
    : public ::testing::TestWithParam<std::pair<std::int32_t, std::uint32_t>>
{
};

TEST_P(PointerArithmeticAddSignedIntAndUnsignedIntPreconditionViolationFixture,
       WhenAddingUnsignedIntToSignedIntWhichWouldCauseOverflowProgramTerminates)
{
    const auto [signed_value, unsigned_value] = GetParam();

    // When adding an unsigned integer to a signed integer that would lead to an overflow of the signed integer
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(AddUnsignedToSigned(signed_value, unsigned_value));
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticAddSignedIntAndUnsignedIntPreconditionViolationFixture,
                         PointerArithmeticAddSignedIntAndUnsignedIntPreconditionViolationFixture,
                         ::testing::Values(std::make_pair(1, kInt32Max),
                                           std::make_pair(-1, kInt32Max + 2U),
                                           std::make_pair(kInt32Max, 1U),
                                           std::make_pair(kInt32Min + 1, kUInt32Max)));

class PointerArithmeticAddSignedInt8AndUnsignedInt8PreconditionViolationFixture
    : public ::testing::TestWithParam<std::pair<std::int8_t, std::uint8_t>>
{
};

TEST_P(PointerArithmeticAddSignedInt8AndUnsignedInt8PreconditionViolationFixture,
       WhenAddingUnsignedIntToSignedIntWhichWouldCauseOverflowProgramTerminates)
{
    const auto [signed_value, unsigned_value] = GetParam();

    // When adding an unsigned integer to a signed integer that would lead to an overflow of the signed integer
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(AddUnsignedToSigned(signed_value, unsigned_value));
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticAddSignedInt8AndUnsignedInt8PreconditionViolationFixture,
                         PointerArithmeticAddSignedInt8AndUnsignedInt8PreconditionViolationFixture,
                         ::testing::Values(std::make_pair(1, 127U),
                                           std::make_pair(-1, 129U),
                                           std::make_pair(127, 1U),
                                           std::make_pair(-127, 255U)));

class PointerArithmeticSubtractUnsignedIntFromSignedIntFixture
    : public ::testing::TestWithParam<std::tuple<std::int32_t, std::uint32_t, std::int32_t>>
{
};

TEST_P(PointerArithmeticSubtractUnsignedIntFromSignedIntFixture,
       WhenSubtractingUnsignedIntFromSignedIntReturnsCorrectValueAsSignedInt)
{
    const auto [signed_value, unsigned_value, expected_result] = GetParam();

    // When subtracting an unsigned integer from a signed integer
    const auto actual_result = SubtractUnsignedFromSigned(signed_value, unsigned_value);

    // Then the resulting signed integer should equal the expected value
    EXPECT_EQ(actual_result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticSubtractUnsignedIntFromSignedIntFixture,
                         PointerArithmeticSubtractUnsignedIntFromSignedIntFixture,
                         ::testing::Values(std::make_tuple(0, 0U, 0),
                                           std::make_tuple(0, 1U, -1),
                                           std::make_tuple(1, 0U, 1),

                                           std::make_tuple(10, 20U, -10),
                                           std::make_tuple(20, 10U, 10),
                                           std::make_tuple(0, kInt32Max, kInt32Max * -1),
                                           std::make_tuple(9, kInt32Max + 10U, kInt32Min),
                                           std::make_tuple(-1, kInt32Max, kInt32Min),

                                           std::make_tuple(-1, 0U, -1),
                                           std::make_tuple(10, 10U, 0),
                                           std::make_tuple(-10, 9U, -19),
                                           std::make_tuple(kInt32Max, kInt32Max, 0)));

class PointerArithmeticSubtractUnsignedInt8FromSignedInt8Fixture
    : public ::testing::TestWithParam<std::tuple<std::int8_t, std::uint8_t, std::int8_t>>
{
};

TEST_P(PointerArithmeticSubtractUnsignedInt8FromSignedInt8Fixture,
       WhenSubtractingUnsignedInt8FromSignedInt8ReturnsCorrectValueAsSignedInt8)
{
    const auto [signed_value, unsigned_value, expected_result] = GetParam();

    // When subtracting an unsigned 8 bit integer from a signed 8 bit integer
    const auto actual_result = SubtractUnsignedFromSigned(signed_value, unsigned_value);

    // Then the resulting signed 8 bit integer should equal the expected value
    EXPECT_EQ(actual_result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticSubtractUnsignedInt8FromSignedInt8Fixture,
                         PointerArithmeticSubtractUnsignedInt8FromSignedInt8Fixture,
                         ::testing::Values(std::make_tuple(0, 0U, 0),
                                           std::make_tuple(0, 1U, -1),
                                           std::make_tuple(1, 0U, 1),

                                           std::make_tuple(10, 20U, -10),
                                           std::make_tuple(20, 10U, 10),
                                           std::make_tuple(0, 127U, -127),
                                           std::make_tuple(0, 128U, -128),
                                           std::make_tuple(9, 137U, -128),
                                           std::make_tuple(-1, 127U, -128),

                                           std::make_tuple(-1, 0U, -1),
                                           std::make_tuple(10, 10U, 0),
                                           std::make_tuple(-10, 9U, -19),
                                           std::make_tuple(127, 127U, 0)));

class PointerArithmeticSubtractUnsignedIntFromSignedIntPreconditionViolationFixture
    : public ::testing::TestWithParam<std::pair<std::int32_t, std::uint32_t>>
{
};

TEST_P(PointerArithmeticSubtractUnsignedIntFromSignedIntPreconditionViolationFixture,
       WhenSubtractingUnsignedTypeFromSignedTypeWhichWouldCauseProgramTerminates)
{
    const auto [signed_value, unsigned_value] = GetParam();

    // When subtracting an unsigned integer from a signed integer that would lead to an underflow of the signed integer
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(SubtractUnsignedFromSigned(signed_value, unsigned_value));
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticSubtractUnsignedIntFromSignedIntPreconditionViolationFixture,
                         PointerArithmeticSubtractUnsignedIntFromSignedIntPreconditionViolationFixture,
                         ::testing::Values(std::make_pair(-2, kInt32Max),
                                           std::make_pair(kInt32Min, 1),
                                           std::make_pair(kInt32Max - 1, kUInt32Max)));

class PointerArithmeticSubtractUnsignedInt8FromSignedInt8PreconditionViolationFixture
    : public ::testing::TestWithParam<std::pair<std::int8_t, std::uint8_t>>
{
};

TEST_P(PointerArithmeticSubtractUnsignedInt8FromSignedInt8PreconditionViolationFixture,
       WhenSubtractingUnsignedTypeFromSignedTypeWhichWouldCauseProgramTerminates)
{
    const auto [signed_value, unsigned_value] = GetParam();

    // When subtracting an unsigned integer from a signed integer that would lead to an underflow of the signed integer
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(SubtractUnsignedFromSigned(signed_value, unsigned_value));
}

INSTANTIATE_TEST_SUITE_P(PointerArithmeticSubtractUnsignedInt8FromSignedInt8PreconditionViolationFixture,
                         PointerArithmeticSubtractUnsignedInt8FromSignedInt8PreconditionViolationFixture,
                         ::testing::Values(std::make_pair(-2, 127U),
                                           std::make_pair(-128, 1U),
                                           std::make_pair(126, 255U)));

}  // namespace score::memory::shared::test
