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
#include "score/memory/shared/double_inverse_stored_int.h"

#include "gtest/gtest.h"
#include <climits>
#include <cstdint>
#include <numeric>
#include <random>

namespace score
{
namespace memory
{
namespace shared
{

template <typename IntType>
class DoubleInverseStoredIntAttorney
{
  public:
    DoubleInverseStoredIntAttorney(DoubleInverseStoredInt<IntType>& double_inverse_stored_int)
        : double_inverse_stored_int_{double_inverse_stored_int}
    {
    }

    void SetOriginalValue(const IntType value)
    {
        double_inverse_stored_int_.original_ = value;
    }
    void SetMirroredValue(const IntType value)
    {
        double_inverse_stored_int_.mirrored_ = value;
    }

  private:
    DoubleInverseStoredInt<IntType>& double_inverse_stored_int_;
};

namespace
{

template <typename IntType>
IntType GenerateRandomInt()
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<IntType> dist6(0, std::numeric_limits<IntType>::max());
    return dist6(rng);
}

}  // namespace

namespace test
{

TEST(DoubleInverseStoredIntTest, InitialisingWithoutCorruptionReturnsSuccessfullyUInt16)
{
    using IntType = std::uint16_t;
    std::vector<IntType> test_values{std::numeric_limits<IntType>::max()};
    std::iota(test_values.begin(), test_values.end(), 0);
    for (auto test_value : test_values)
    {
        const DoubleInverseStoredInt<IntType> double_inverse_stored_int{test_value};
        EXPECT_EQ(test_value, double_inverse_stored_int.GetValue());
    }
}

TEST(DoubleInverseStoredIntTest, InitialisingWithoutCorruptionReturnsSuccessfully)
{
    using IntType = std::uint64_t;
    const int num_test_values{100};
    for (int i = 0; i < num_test_values; ++i)
    {
        const IntType test_value = GenerateRandomInt<IntType>();
        const DoubleInverseStoredInt<IntType> double_inverse_stored_int{test_value};
        EXPECT_EQ(test_value, double_inverse_stored_int.GetValue());
    }
}

TEST(DoubleInverseStoredIntTest, SettingValueWithoutCorruptionReturnsSuccessfullyUInt16)
{
    using IntType = std::uint16_t;
    std::vector<IntType> original_values{std::numeric_limits<IntType>::max()};
    std::iota(original_values.begin(), original_values.end(), 0);
    for (auto test_value : original_values)
    {
        DoubleInverseStoredInt<IntType> double_inverse_stored_int{1};

        double_inverse_stored_int.SetValue(test_value);
        EXPECT_EQ(test_value, double_inverse_stored_int.GetValue());
    }
}

TEST(DoubleInverseStoredIntTest, SettingValueWithoutCorruptionReturnsSuccessfully)
{
    using IntType = std::uint64_t;

    const int num_test_values{100};
    for (int i = 0; i < num_test_values; ++i)
    {
        DoubleInverseStoredInt<IntType> double_inverse_stored_int{1};

        const IntType test_value = GenerateRandomInt<IntType>();
        double_inverse_stored_int.SetValue(test_value);
        EXPECT_EQ(test_value, double_inverse_stored_int.GetValue());
    }
}

TEST(DoubleInverseStoredIntDeathTest, InitialisingWithCorruptionOfOriginalTerminates)
{
    RecordProperty("Verifies", "SCR-6223631");
    RecordProperty("Description",
                   "The MemoryRessourceProxy shall store its identifier in a way, that it can detect corruptions.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using IntType = std::uint64_t;

    const IntType original_value{100U};
    const IntType corrupt_original_value{101U};

    DoubleInverseStoredInt<IntType> double_inverse_stored_int(original_value);
    DoubleInverseStoredIntAttorney<IntType> double_inverse_stored_int_attorney(double_inverse_stored_int);

    double_inverse_stored_int_attorney.SetOriginalValue(corrupt_original_value);
    EXPECT_DEATH(double_inverse_stored_int.GetValue(), ".*");
}

TEST(DoubleInverseStoredIntDeathTest, InitialisingWithCorruptionOfMirroredTerminates)
{
    RecordProperty("Verifies", "SCR-6223631");
    RecordProperty("Description",
                   "The MemoryRessourceProxy shall store its identifier in a way, that it can detect corruptions.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    using IntType = std::uint64_t;

    const IntType original_value{100U};
    const IntType corrupt_mirrored_value{101U};

    DoubleInverseStoredInt<IntType> double_inverse_stored_int(original_value);
    DoubleInverseStoredIntAttorney<IntType> double_inverse_stored_int_attorney(double_inverse_stored_int);

    double_inverse_stored_int_attorney.SetMirroredValue(corrupt_mirrored_value);
    EXPECT_DEATH(double_inverse_stored_int.GetValue(), ".*");
}

}  // namespace test

}  // namespace shared
}  // namespace memory
}  // namespace score
