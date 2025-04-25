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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_DOUBLEINVERSESTOREDINT_H
#define BASELIBS_SCORE_MEMORY_SHARED_DOUBLEINVERSESTOREDINT_H

#include <climits>
#include <exception>

namespace score
{
namespace memory
{
namespace shared
{

/**
 * \brief The `DoubleInverseStoredInt` is used to protect a safety related integer value. It is passed the value during
 * construction or via a setter and will call std::terminate() if the value retrieved is ever
 * different from the passed value.
 *
 * It achieves this by saving a copy of the variable with mirrored bits and checking on every read whether re-mirroring
 * the originally calculated mirrored value is the same as the original value to be returned.
 *
 * E.g. DoubleInverseStoredInt<std::uint8_t> int_value{100}:
 *  binary representation of original_ == 0b0110 0100
 *  binary representation of mirrored_ == 0b0010 0110
 *
 *  When GetValue() is called, it will ensure that original_ == MirrorIntBits(mirrored_)
 */
template <typename IntType>
class DoubleInverseStoredInt
{
  public:
    explicit DoubleInverseStoredInt(const IntType init_value)
        : original_{init_value}, mirrored_{MirrorIntBits(init_value)}
    {
    }
    ~DoubleInverseStoredInt() = default;
    DoubleInverseStoredInt(const DoubleInverseStoredInt&) = default;
    DoubleInverseStoredInt& operator=(const DoubleInverseStoredInt&) = default;
    DoubleInverseStoredInt(DoubleInverseStoredInt&&) noexcept = default;
    DoubleInverseStoredInt& operator=(DoubleInverseStoredInt&&) noexcept = default;

    void SetValue(const IntType value) noexcept
    {
        original_ = value;
        mirrored_ = MirrorIntBits(value);
    }

    IntType GetValue() const noexcept
    {
        const auto remirrored = MirrorIntBits(mirrored_);
        if (remirrored != original_)
        {
            std::terminate();
        }
        return original_;
    }

  private:
    IntType MirrorIntBits(IntType original) const noexcept
    {
        const size_t num_bits = sizeof(original) * CHAR_BIT;
        const IntType first_bit_mask{1};

        IntType mirrored{0};

        // Iterate over each bit except the last
        for (size_t i = 0; i < num_bits - 1; ++i)
        {
            // Assign the current least significant bit of the original value to the least significant bit of the
            // mirrored value. E.g. original{1001} & mirrored{0000} -> mirrored{0001}
            mirrored |= (original & first_bit_mask);

            // Left bit shift the mirrored value: mirrored{0001} -> mirrored{0010}
            // If IntType is smaller than int, then integer promotion will cause mirrored to be implicitly cast to an
            // int and therefore the value returned by the bit shifting operation will be an int. We then get a compiler
            // warning when implicitly casting the int back to the smaller type IntType. Therefore, we explicitly
            // perform this cast since we know that the original value was of type IntType so no information will be
            // lost.
            mirrored = static_cast<IntType>(mirrored << 1U);

            // Right bit shift the original value: original{1001} -> original{0100}
            original = static_cast<IntType>(original >> 1U);
        }

        // Assign the last digit but don't bit shift the result.
        mirrored |= (original & first_bit_mask);

        return mirrored;
    }

    // Friend class used for testing
    template <typename T>
    friend class DoubleInverseStoredIntAttorney;

    IntType original_;
    IntType mirrored_;
};

}  // namespace shared
}  // namespace memory
}  // namespace score

#endif  // BASELIBS_SCORE_MEMORY_SHARED_DOUBLEINVERSESTOREDINT_H
