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
#ifndef SCORE_LIB_MEMORY_SHARED_POINTER_ARITHMETIC_UTIL_H
#define SCORE_LIB_MEMORY_SHARED_POINTER_ARITHMETIC_UTIL_H

#include "score/memory/data_type_size_info.h"

#include "score/mw/log/log_types.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

namespace score::memory::shared
{

namespace detail
{

template <typename T>
constexpr bool is_signed_integer_v = std::conjunction_v<std::is_integral<T>, std::is_signed<T>>;

template <typename T>
constexpr bool is_unsigned_integer_v = std::conjunction_v<std::is_integral<T>, std::is_unsigned<T>>;

}  // namespace detail

std::uintptr_t AddOffsetToPointerAsInteger(const std::uintptr_t pointer_as_integer, const std::size_t offset);
std::uintptr_t AddSignedOffsetToPointerAsInteger(const std::uintptr_t ptr_as_integer, const std::ptrdiff_t offset);
std::uintptr_t SubtractOffsetFromPointerAsInteger(const std::uintptr_t pointer_as_integer, const std::size_t offset);

/// \brief Calculates the needed size to store an object of given size, so it shall end at an address with the given
///        alignment.
/// \details Effectively this function calculates the value of size + eventually needed minimal padding, so that the
///          next byte after size + padding is aligned to the given alignment. This obviously presumes, that the memory
///          address, where the object for which we calculate "aligned size" is stored, is both: Aligned for the needs
///          of the object itself and fulfilling the needs of alignment
/// \param size size of the object (e.g. via sizeof)
/// \param alignment required alignment
/// \return Aligned size of the object with the given size (size + minimal trailing padding)
constexpr std::size_t CalculateAlignedSize(const std::size_t size, const std::size_t alignment) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(alignment != 0U, "Division by zero is undefined!!");

    if ((size % alignment) == 0U)
    {
        return size;
    }
    if (size > alignment)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(size <= (std::numeric_limits<std::size_t>::max() - alignment),
                               "Addition would overflow!!");
        return size + alignment - (size % alignment);
    }
    return alignment;
}

/// \brief Calculates the needed size to store a sequence of objects of given size contiguously in memory.
///
/// \details Effectively this function calculates the value of size + eventually needed minimal padding between each
/// element. Assumes that the allocation starts at a location that has worst case alignment. E.g. if you were to
/// allocate memory for the first DataTypeSizeInfo, it would be allocated without any padding before the element. Note.
/// Does not calculate any padding after the last element, as this would require knowledge of the alignment of the next
/// object in memory.
///
/// \param data_type_infos vector of DataTypeSizeInfos which contains size and alignment of each element
/// \return Required memory to allocate all objects (i.e. size of each object plus padding in between elements).
std::size_t CalculateAlignedSizeOfSequence(const std::vector<DataTypeSizeInfo>& data_type_infos);

/// \brief Casts a pointer to an integer.
///
/// According to https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#4, this is implementation defined.
std::uintptr_t CastPointerToInteger(const void* const pointer) noexcept;

/// \brief Casts an integer to a pointer.
///
/// According to https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#5, this is implementation defined.
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.
// Rationale: False positive CastIntegerToPointer is declared only once.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
template <typename PointerType>
auto CastIntegerToPointer(std::uintptr_t integer) noexcept -> PointerType
{
    // Suppress "AUTOSAR C++14 A5-2-4" rule finding: reinterpret_cast shall not be used.
    // Rationale : According to https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#5, casting an integer to
    // a pointer is implementation-defined. We rely on sufficient testing to ensure that the implementation defined
    // behaviour performs as expected (i.e. the address of the resulting pointer is the same as the integer value).
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): see rationale above.
    // coverity[autosar_cpp14_a5_2_4_violation]
    return reinterpret_cast<PointerType>(integer);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
}

/// \brief Calculates the pointer resulting from adding an offset to a pointer
///
/// Adding an offset to a pointer directly may result in undefined behaviour depending on whether the resulting pointer
/// points within the same array as the original pointer.This function avoids undefined
/// behaviour by first casting the address to an integral type, adding the offset to the integer and then casting the
/// resulting integer back to a pointer. The conversion of a pointer to an integral type and an integral type to a
/// pointer are implementation defined: https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#4 and
/// https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#5, respectively.
template <typename PointerType>
auto AddOffsetToPointer(const PointerType* pointer, const std::size_t offset) -> PointerType*
{
    const auto pointer_as_integer = CastPointerToInteger(pointer);
    const auto result_as_integer = AddOffsetToPointerAsInteger(pointer_as_integer, offset);
    // NOLINTNEXTLINE(score-banned-function): This function is banned for calling CastIntegerToPointer
    return CastIntegerToPointer<PointerType*>(result_as_integer);
}

template <typename PointerType>
auto AddOffsetToPointer(const PointerType* pointer, const std::ptrdiff_t offset) -> PointerType*
{
    const auto pointer_as_integer = CastPointerToInteger(pointer);
    const auto result_as_integer = AddSignedOffsetToPointerAsInteger(pointer_as_integer, offset);
    // NOLINTNEXTLINE(score-banned-function): This function is banned for calling CastIntegerToPointer
    return CastIntegerToPointer<PointerType*>(result_as_integer);
}

score::mw::log::LogHex64 PointerToLogValue(const void* const pointer);

/// \brief Calculates the number of bytes by subtracting the second address from the first address.
///
/// Subtracting pointers directly may result in undefined behaviour depending on whether the pointers are pointing to
/// elements of the same array. This function avoids undefined behaviour by first casting the addresses to integral
/// types and subtracting the integers. The conversion of a pointer to an integral type is implementation defined:
/// https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#4
std::ptrdiff_t SubtractPointersBytes(const void* const first, const void* const second) noexcept;

/// \brief Calculates the absolute value of a signed integer and returns the result as an unsigned integer with the same
/// bit width
///
/// std::abs returns the same signed type as the type provided. Therefore, std::numeric_limits<SignedInteger>::min()
/// will result in undefined behaviour, since the absolute value would be std::numeric_limits<SignedInteger>::max() + 1U
template <typename SignedInteger, typename = std::enable_if_t<detail::is_signed_integer_v<SignedInteger>>>
std::make_unsigned_t<SignedInteger> AbsoluteValue(SignedInteger signed_value)
{
    using UnsignedInteger = std::make_unsigned_t<SignedInteger>;

    // We have to handle the special case in which signed_value == SignedValue::min. In such a case, the absolute value
    // of the result will be SignedValue::max + 1U which cannot fit inside a SignedValue. So we directly return
    // SignedValue::max + 1U in such a case.
    if (signed_value == std::numeric_limits<SignedInteger>::min())
    {
        return static_cast<UnsignedInteger>(std::numeric_limits<SignedInteger>::max()) + 1U;
    }
    return static_cast<UnsignedInteger>(std::abs(signed_value));
}

/// \brief Undo a cast from an unsigned integer to a signed integer i.e. Casts unsigned integer to signed integer of
/// same bit width
///
/// This function casts an unsigned integer to a signed integer such that signed_integer ==
/// UndoSignedToUnsignedIntegerCast(static_cast<UnsignedInteger>(signed_integer)) for all possible values of
/// signed_integer.
///
/// Note. Casting an unsigned integer to a signed integer for values larger than the maximum signed value is
/// implementation defined so should be avoided (https://timsong-cpp.github.io/cppwp/n4659/conv.integral#3). Casting an
/// unsigned integer to a signed integer is fully defined (https://timsong-cpp.github.io/cppwp/n4659/conv.integral#2)
template <typename UnsignedInteger, typename = std::enable_if_t<detail::is_unsigned_integer_v<UnsignedInteger>>>
std::make_signed_t<UnsignedInteger> UndoSignedToUnsignedIntegerCast(UnsignedInteger unsigned_value)
{
    using SignedInteger = std::make_signed_t<UnsignedInteger>;

    constexpr auto signed_min = std::numeric_limits<SignedInteger>::min();
    constexpr auto signed_max = std::numeric_limits<SignedInteger>::max();

    // Result will be positive and fits inside a SignedInteger so we can directly cast which is defined by the standard.
    if (unsigned_value <= static_cast<UnsignedInteger>(signed_max))
    {
        return static_cast<SignedInteger>(unsigned_value);
    }

    // If unsigned_value > SignedInteger::max, it means that the resulting signed value will be negative. A direct cast
    // will be implementation defined. Since casting a signed type to unsigned is defined by the standard and will
    // return the signed type modulo UnsignedInteger::max, we know that SignedInteger::max + 1 ==
    // UnsignedInteger(SignedInteger::min). Therefore, we can shift the unsigned value to be between 0 and
    // SignedInteger::max by subtracting UnsignedInteger(SignedInteger::min). We can then add it back using
    // SignedInteger::min after casting the UnsignedInteger to a SignedInteger.
    const auto shifted_unsigned_value = unsigned_value - static_cast<UnsignedInteger>(signed_min);

    // Perform explicit cast on the result since for SignedIntegers smaller than int, the result of the addition will be
    // an integer.
    return static_cast<SignedInteger>(static_cast<SignedInteger>(shifted_unsigned_value) + signed_min);
}

/// \brief Add an unsigned integer to a signed integer of same bit width
///
/// This function relies on the fact that casting a signed integer to an unsigned integer is defined by the standard
/// (https://timsong-cpp.github.io/cppwp/n4659/conv.integral#2). Since the wrapping of overflowing unsigned values is
/// also defined by the standard, we add the unsigned type to an unsigned representation of the signed type and then
/// cast the result back to a signed type.
template <typename SignedInteger, typename = std::enable_if_t<detail::is_signed_integer_v<SignedInteger>>>
auto AddUnsignedToSigned(SignedInteger signed_value, std::make_unsigned_t<SignedInteger> unsigned_value)
    -> SignedInteger
{
    using UnsignedInteger = std::make_unsigned_t<SignedInteger>;

    // Cast the signed integer to an unsigned integer. The behaviour is defined by the standard.
    const auto unsigned_representation_of_signed_value = static_cast<UnsignedInteger>(signed_value);

    // Perform explicit cast on the result since for SignedIntegers smaller than int, the result of the addition will be
    // an integer.
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to loss.".
    // Rationale: Adding unsigned integers will result in wrapping. This is intended by the algorithm, as
    // UndoSignedToUnsignedIntegerCast will handle the unwrapping to get the correct signed result.
    // coverity[autosar_cpp14_a4_7_1_violation]
    const auto unsigned_result = static_cast<UnsignedInteger>(unsigned_representation_of_signed_value + unsigned_value);

    // Cast the unsigned result to a signed result. This function ensures that it uses only behaviour defined by the
    // standard.
    const auto signed_result = UndoSignedToUnsignedIntegerCast(unsigned_result);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(signed_result >= signed_value,
                           "If signed_result is smaller than signed_value, it indicates that an overflow ocurred due "
                           "to signed_value + unsigned_value not fitting into SignedInteger type.");
    return signed_result;
}

template <typename SignedInteger, typename = std::enable_if_t<detail::is_signed_integer_v<SignedInteger>>>
auto SubtractUnsignedFromSigned(SignedInteger signed_value, std::make_unsigned_t<SignedInteger> unsigned_value)
    -> SignedInteger
{
    using UnsignedInteger = std::make_unsigned_t<SignedInteger>;

    // Cast the signed integer to an unsigned integer. The behaviour is defined by the standard.
    const auto unsigned_representation_of_signed_value = static_cast<UnsignedInteger>(signed_value);

    // Perform explicit cast on the result since for SignedIntegers smaller than int, the result of the addition will be
    // an integer.
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to loss.".
    // Rationale: Subtracting unsigned integers will result in wrapping. This is intended by the algorithm, as
    // UndoSignedToUnsignedIntegerCast will handle the unwrapping to get the correct signed result.
    // coverity[autosar_cpp14_a4_7_1_violation]
    const auto unsigned_result = static_cast<UnsignedInteger>(unsigned_representation_of_signed_value - unsigned_value);

    // Cast the unsigned result to a signed result. This function ensures that it uses only behaviour defined by the
    // standard.
    const auto signed_result = UndoSignedToUnsignedIntegerCast(unsigned_result);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(signed_result <= signed_value,
                           "If signed_result is larger than signed_value, it indicates that an underflow ocurred due "
                           "to signed_value - unsigned_value not fitting into SignedInteger type.");
    return signed_result;
}

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_POINTER_ARITHMETIC_UTIL_H
