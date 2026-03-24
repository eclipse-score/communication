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
#ifndef SCORE_LIB_MEMORY_SHARED_OFFSET_PTR_H
#define SCORE_LIB_MEMORY_SHARED_OFFSET_PTR_H

#include "score/memory/shared/memory_region_bounds.h"
#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/offset_ptr_bounds_check.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include "score/language/safecpp/safe_math/safe_math.h"
#include "score/quality/compiler_warnings/warnings.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/overload.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

// Justification for usage of banned functions:
// An offset pointer is an alternative representation of a pointer type where a pointer is represented by a base pointer
// and an offset.
// The address of the OffsetPtr is the base pointer and the offset is stored within the OffsetPtr as an integral type.
// The offset is calculated by:
//   1. Cast the base pointer and original pointer to integers
//   2. Subtract the base pointer (integer representation) from the original pointer (integer representation)
// This is possible because in our architecture we have a one-to-one mapping between pointers and integral values.
// To reconstruct the pointer back, we need to follow a three-step process:
//   1. Cast the base pointer to an integer
//   2. Calculate the sum of the base pointer (integer representation) and offset
//   3. Cast the result form the previous step to a pointer type
// In our case, this only happens in the context of a round trip (T* -> (base, offset) -> T*).
// Because of that, the behavior is specified to be the same as original provided pointer.
//
// Note: Whenever a OffsetPtr is copied or moved the offset is recalculated to ensure that the
// resulting pointer still matches the originally provided one by:
//   1. Recalculating the original pointer (following above process)
//   2. Calculating the offset based on the new base pointer (following above process)

namespace score::memory::shared
{

namespace detail_offset_ptr
{

using difference_type = std::ptrdiff_t;

// Alias for checking if template types are of the same type after stripping const/volatile qualifiers.
template <typename T1, typename T2>
using cv_stripped_types_equal =
    typename std::is_same<typename std::remove_cv<T1>::type, typename std::remove_cv<T2>::type>;

/// \brief SFINAE helpers for enabling a function if the template type provided is (not) void.
///
/// This helper is used for methods of OffsetPtr which is a template class. Since SFINAE only works for function
/// template parameters, each class method must create a function template parameter (usually defaulted with the
/// OffsetPtr's template type). However, this type can be "hijacked" by calling the method with an explicit template
/// type e.g. offset_ptr.template get<SomeOtherType>(). Therefore, the function template parameter AND the OffsetPtr's
/// template type should be provided here as T and PointedType, respectively. This helper will only return true if
/// they're both the same.
template <typename T, typename PointedType>
using enable_if_type_is_void = typename std::enable_if_t<
    std::conjunction_v<std::is_same<std::remove_cv_t<T>, void>, std::is_same<T, PointedType>>>;
template <typename T, typename PointedType>
using enable_if_type_is_not_void = typename std::enable_if_t<
    std::conjunction_v<std::negation<std::is_same<std::remove_cv_t<T>, void>>, std::is_same<T, PointedType>>>;

/// @brief Offset value used by OffsetPtr to represent a null pointer.
constexpr difference_type kNullPtrRepresentation = 1;

bool IsBoundsCheckingEnabled() noexcept;

}  // namespace detail_offset_ptr

/// \brief Enables/Disables OffsetPtr bounds-checking globally. Initially it is activated for safety reasons!
/// \details Bounds-checking involves some overhead, whenever "interacting" with an OffsetPtr
///          (deref, changing, copying, ..). In an ASIL-QM environment this overhead isn't necessary, so in this case
///          it can be deactivated.
/// \param enable true - enables bounds-checking, false - disables it
/// \return previous value of bounds-check-enabled
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// this is false positive. EnableOffsetPtrBoundsChecking is declared only once.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
bool EnableOffsetPtrBoundsChecking(const bool enable);

/// \brief Custom implementation of an offset pointer (aka fancy pointer or relative pointer). When in a
/// ManagedMemoryResource, an offset pointer stores the memory address of an object with respect to the offset pointer
/// itself. Otherwise, it stores a regular pointer.
///
/// This is particularly useful (and required) when dealing with pointers in shared memory. Since different
/// processes can map the same shared memory region to different addresses, an absolute pointer to an object in shared
/// memory is only valid for the process which created the object. If we instead use an offset pointer, then each
/// process can find the memory address of the pointed-to object in its own memory mapped region.
///
/// ATTENTION: It is up to the user to verify the validity of the pointer before dereferencing it!
/// E.g. make sure that the object is of type T (or derived) and not yet destructed or moved-from.
///
/// The full documentation of OffsetPtr is contained within
/// score/memory/design/shared_memory/OffsetPtrDesign.md
template <typename PointedType>

// Suppress "AUTOSAR C++14 A12-0-1", The rule states: "If a class declares a copy or move operation, or a destructor,
// either via "=default", "=delete", or via a user-provided declaration, then all others of these five special member
// functions shall be declared as well."
// Rationale: A move constructor and move assignment operator are not implemented so that the copy constructor /
// assignment operators are used when moving an OffsetPtr. There would be no performance benefit of a move constructor /
// assignment operator since whether we move or copy, we still need to re-calculate the memory offset.
// Suppress "AUTOSAR C++14 M3-2-3", The rule states: "A type, object or function that is used in multiple translation
// units shall be declared in one and only one file."
// Rationale: this is false positive. OffsetPtr is declared only once.
// NOLINTBEGIN(cppcoreguidelines-special-member-functions): Discard movement operations.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
// coverity[autosar_cpp14_a12_0_1_violation]
class OffsetPtr
// NOLINTEND(cppcoreguidelines-special-member-functions): see above
{
    template <typename>
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design decision: OffsetPtrs templated with any type should be able to access all other OffsetPtrs so that the
    // converting copy constructors can access the private members of the other OffsetPtr. E.g. when creating
    // OffsetPtr<T> from OffsetPtr<T2>.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class OffsetPtr;

  public:
    // For the std::pointer_traits interface.
    using pointer = PointedType*;
    using const_pointer = std::add_pointer_t<std::add_const_t<std::remove_pointer_t<pointer>>>;
    using element_type = PointedType;
    using difference_type = detail_offset_ptr::difference_type;

    // For the std::iterator_traits interface.
    using value_type = std::remove_cv_t<PointedType>;
    using reference = std::add_lvalue_reference_t<PointedType>;
    using iterator_category = std::random_access_iterator_tag;

    template <class U>
    using rebind = OffsetPtr<U>;

    // Suppress "AUTOSAR C++14 A12-1-4" rule finding: "All constructors that are callable with a single argument of
    // fundamental type shall be declared explicit.
    // Rationale : Non-explicit constructor is needed for implicit conversion
    // NOLINTBEGIN(google-explicit-constructor): needed implicit conversion
    // coverity[autosar_cpp14_a12_1_4_violation]
    OffsetPtr(pointer ptr = nullptr) noexcept;
    // NOLINTEND(google-explicit-constructor): see above

    OffsetPtr(const OffsetPtr<PointedType>& other);
    OffsetPtr& operator=(pointer ptr);
    OffsetPtr& operator=(const OffsetPtr& other);

    // Suppress "AUTOSAR C++14 A12-1-4" rule finding: "All constructors that are callable with a single argument of
    // fundamental type shall be declared explicit.
    // Rationale : Non-explicit converting constructor is needed for implicit conversion
    // NOLINTBEGIN(google-explicit-constructor): needed implicit conversion
    // coverity[autosar_cpp14_a12_1_4_violation]
    template <typename OtherPointedType>
    OffsetPtr(const OffsetPtr<OtherPointedType>& other);
    // NOLINTEND(google-explicit-constructor): see above

    template <typename OtherPointedType>
    OffsetPtr& operator=(const OffsetPtr<OtherPointedType>& other);

    // Enabled if PointedType is not void
    template <class T = PointedType, class = detail_offset_ptr::enable_if_type_is_not_void<T, PointedType>>
    // Suppress "AUTOSAR C++14 A7-1-8", The rule states: "A non-type specifier shall be placed before a type specifier
    // in a declaration.".
    // Rationale: False positive: all non-type specifiers are before any type specifiers.
    // coverity[autosar_cpp14_a7_1_8_violation : FALSE]
    static OffsetPtr pointer_to(std::add_lvalue_reference_t<T> ref) noexcept;

    // Enabled if PointedType is void
    template <class T = PointedType, class = detail_offset_ptr::enable_if_type_is_void<T, PointedType>>
    // coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
    static OffsetPtr pointer_to(void* const r) noexcept;

    // Enabled if PointedType is void
    template <class T = PointedType, class = detail_offset_ptr::enable_if_type_is_void<T, PointedType>>
    // coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
    static OffsetPtr pointer_to(const void* const r) noexcept;

    reference operator*() const;
    reference operator[](difference_type idx) const;

    OffsetPtr& operator+=(difference_type offset);
    OffsetPtr& operator-=(difference_type offset);
    OffsetPtr& operator++();
    OffsetPtr operator++(std::int32_t);
    OffsetPtr& operator--();
    OffsetPtr operator--(std::int32_t);

    template <class T = PointedType, class = detail_offset_ptr::enable_if_type_is_not_void<T, PointedType>>
    pointer get() const
    {
        // NOLINTNEXTLINE(score-banned-function) See justification above class.
        return GetPointerWithBoundsCheck(this, offset_, memory_bounds_, sizeof(PointedType));
    }

    template <typename ExplicitPointedType,
              class T = PointedType,
              class = detail_offset_ptr::enable_if_type_is_void<T, PointedType>>
    auto get() const -> ExplicitPointedType*
    {
        // Suppress "AUTOSAR C++14 M5-2-8" rule finding:  An object with integer type or pointer to void type shall not
        // be converted to an object with pointer type.
        // Rationale : This function is only enabled when the pointed type is type erased (i.e. void). It's intended
        // that the type erased pointer will be cast back to the original type. Casting a pointer to void and back to
        // the original type is defined behaviour. It's up to the user of the library to ensure that ExplicitPointedType
        // is the same as the original type before casting to a void pointer.
        // coverity[autosar_cpp14_m5_2_8_violation]
        return static_cast<ExplicitPointedType*>(
            // NOLINTNEXTLINE(score-banned-function) See justification above class.
            GetPointerWithBoundsCheck(this, offset_, memory_bounds_, sizeof(ExplicitPointedType)));
    }

    template <class T = PointedType, class = detail_offset_ptr::enable_if_type_is_void<T, PointedType>>
    pointer get(const std::size_t explicit_pointed_type_size) const
    {
        // NOLINTNEXTLINE(score-banned-function) See justification above class.
        return GetPointerWithBoundsCheck(this, offset_, memory_bounds_, explicit_pointed_type_size);
    }

    template <class T = PointedType, class = detail_offset_ptr::enable_if_type_is_not_void<T, PointedType>>
    // Suppress "AUTOSAR C++14 A13-5-2" rule finding:  All user-defined conversion operators shall be defined explicit.
    // Rationale: Using an offset pointer in a basic_string requires this conversion operator to be implicit.
    // NOLINTBEGIN(google-explicit-constructor): requires conversion operator to be implicit
    // coverity[autosar_cpp14_a13_5_2_violation]
    operator pointer() const
    {
        // NOLINTNEXTLINE(score-banned-function) See justification above class.
        return GetPointerWithBoundsCheck(this, offset_, memory_bounds_, sizeof(PointedType));
    }
    // NOLINTEND(google-explicit-constructor): see above

    template <class T = PointedType, class = detail_offset_ptr::enable_if_type_is_not_void<T, PointedType>>
    pointer operator->() const
    {
        // NOLINTNEXTLINE(score-banned-function) See justification above class.
        return GetPointerWithBoundsCheck(this, offset_, memory_bounds_, sizeof(PointedType));
    }

    explicit operator bool() const noexcept
    {
        return offset_ != detail_offset_ptr::kNullPtrRepresentation;
    }

    bool operator!() const noexcept
    {
        return !(operator bool());
    }

  private:
    template <typename OtherPointedType>
    // Suppress "AUTOSAR C++14 A0-1-3" rule finding. This rule states: "Every function defined in an anonymous
    // namespace, or static function with internal linkage, or private member function shall be used.".
    // Rationale: False-positive, this function is used in this file.
    // coverity[autosar_cpp14_a0_1_3_violation : FALSE]
    static std::pair<difference_type, MemoryRegionBounds> CopyFrom(const OffsetPtr<OtherPointedType>& source_offset_ptr,
                                                                   OffsetPtr<PointedType>& target_offset_ptr);

    /// \brief Calculates the absolute pointer of the pointed-to object from the OffsetPtr's variant with a bounds
    /// check.
    ///
    /// Function is static to ensure that it doesn't access any internal member variables while they're running. This is
    /// to prevent other processes corrupting these variables when the OffsetPtr is in shared memory. Any member
    /// variables of OffsetPtr which are provided to the function i.e. offset and memory_resource_identifier should be
    /// provided as copies.
    static pointer GetPointerWithBoundsCheck(const void* const offset_ptr_address,
                                             const difference_type offset,
                                             const MemoryRegionBounds& offset_ptr_memory_bounds_when_not_in_shm,
                                             const std::size_t pointed_type_size);

    /// \brief Calculates the absolute pointer of the pointed-to object from the OffsetPtr's variant without a bounds
    /// check.
    ///
    /// Function is static to ensure that it doesn't access any internal member variables while they're running. This is
    /// to prevent other processes corrupting these variables when the OffsetPtr is in shared memory. Any member
    /// variables of OffsetPtr which are provided to the function i.e. offset should be provided as copies.
    static pointer GetPointerWithoutBoundsCheck(const void* const offset_ptr_address,
                                                const difference_type offset) noexcept;

    void IncrementOffset(const std::size_t multiple_of_pointed_type_to_increment);
    void DecrementOffset(const std::size_t multiple_of_pointed_type_to_decrement);

    static auto CalculateOffsetFromPointer(const void* const offset_ptr_address, pointer pointed_to_address) noexcept
        -> difference_type;

    static auto CalculatePointerFromOffset(const difference_type offset, const void* const offset_ptr) noexcept
        -> pointer;

    /// \brief Offset which represents the number of bytes to the pointed-to object relative to the OffsetPtr's own
    /// address.
    detail_offset_ptr::difference_type offset_;

    /// \brief Memory region bounds used for bounds checking OffsetPtr if it's been copied out of the memory region.
    ///
    /// When an OffsetPtr is in a shared memory region, we can perform BoundsChecks by getting the memory bounds of that
    /// region from the MemoryResourceRegistry using the address of the OffsetPtr. If the OffsetPtr is copied out of the
    /// region, we still need to do these checks before dereferencing the OffsetPtr. We can use these memory bounds
    /// directly in such a case (the memory bounds can only be corrupted by another process when the OffsetPtr is in
    /// shared memory).
    MemoryRegionBounds memory_bounds_;

    // Friend declarations - They need to be friends to be able to get raw pointers without doing bounds checks.
    template <typename T1, typename T2, typename>
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The comparison operators need to get a raw pointer but do not need to do bounds checking. Since we do not want to
    // expose this functionality in the public interface (since it could violate safety goals), we make the operators
    // friends to manually get the raw pointer without bounds checks.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend inline bool operator==(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2);

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline bool operator==(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2);

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline bool operator==(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2);

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline bool operator<(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2);

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline bool operator<(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2);

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline bool operator<(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2);

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline bool operator<=(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2);

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline bool operator<=(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2);

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline bool operator<=(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2);

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline auto operator-(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2) ->
        typename OffsetPtr<T1>::difference_type;

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline auto operator-(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2) ->
        typename OffsetPtr<T1>::difference_type;

    template <typename T1, typename T2, typename>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend inline auto operator-(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2) ->
        typename OffsetPtr<T1>::difference_type;

    template <typename T>
    // coverity[autosar_cpp14_a11_3_1_violation] See rationale for autosar_cpp14_a11_3_1_violation above
    friend void swap(OffsetPtr<T>& left, OffsetPtr<T>& right);
};

// maybe-unitialized is triggered here by some compilers, because they thing that `this` in the initalizer-list
// is maybe unitialized. Fact is, that `this`, in this case, is not derefenced and thus its fine to use it.
// This is also described in the C++ Standard: https://eel.is/c++draft/class.init#class.base.init-15
// NOLINTBEGIN(score-banned-preprocessor-directives) : required due to false positive compiler warning
DISABLE_WARNING_PUSH
DISABLE_WARNING_MAYBE_UNINITIALIZED
template <typename PointedType>
// Suppress "AUTOSAR C++14 M3-9-1" rule finding: "The types used for an object, a function return type, or a function
// parameter shall be token-for-token identical in all declarations and re-declarations."
// Suppress "AUTOSAR C++14 M8-4-2" rule finding: "The identifiers used for the parameters in a re-declaration of a
// function shall be identical to those in the declaration."
// Rationale for autosar_cpp14_m8_4_2_violation and autosar_cpp14_m3_9_1_violation: False positive: All tokens are
// identical between in-class declaration and definition.
// coverity[autosar_cpp14_m8_4_2_violation : FALSE]
// coverity[autosar_cpp14_m3_9_1_violation : FALSE]
OffsetPtr<PointedType>::OffsetPtr(pointer ptr) noexcept
    : offset_{CalculateOffsetFromPointer(this, ptr)}, memory_bounds_{}
{
}
DISABLE_WARNING_POP
// NOLINTEND(score-banned-preprocessor-directives)

template <typename PointedType>
// Suppress AUTOSAR C++14 A12-8-1" rule finding. The rule states "Move and copy constructors shall move and
// respectively copy base classes and data members of a class, without any side effects".
// Rationale: Copying an OffsetPtr requires re-calculating the pointed-to address from the new OffsetPtr, so a simple
// copy of the offset will not work. It also requires different functionality depending on whether it's being copied
// to/from a managed memory region or not.
// coverity[autosar_cpp14_a12_8_1_violation]
OffsetPtr<PointedType>::OffsetPtr(const OffsetPtr<PointedType>& other) : offset_{}, memory_bounds_{}
{
    std::tie(offset_, memory_bounds_) = CopyFrom(other, *this);
}

template <typename PointedType>
template <typename OtherPointedType>
OffsetPtr<PointedType>::OffsetPtr(const OffsetPtr<OtherPointedType>& other) : offset_{}, memory_bounds_{}
{
    std::tie(offset_, memory_bounds_) = CopyFrom<OtherPointedType>(other, *this);
}

template <typename PointedType>
// coverity[autosar_cpp14_m8_4_2_violation : FALSE] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_m3_9_1_violation : FALSE] See rationale for autosar_cpp14_m3_9_1_violation above
auto OffsetPtr<PointedType>::operator=(pointer ptr) -> OffsetPtr&
{
    offset_ = CalculateOffsetFromPointer(this, ptr);
    memory_bounds_.Reset();
    return *this;
}

template <typename PointedType>
// coverity[autosar_cpp14_a6_2_1_violation]: See rationale for copy constructor autosar_cpp14_a12_8_1_violation above
auto OffsetPtr<PointedType>::operator=(const OffsetPtr& other) -> OffsetPtr&
{
    if (this == &other)
    {
        return *this;
    }
    std::tie(offset_, memory_bounds_) = CopyFrom(other, *this);
    return *this;
}

template <typename PointedType>
template <typename OtherPointedType>
auto OffsetPtr<PointedType>::operator=(const OffsetPtr<OtherPointedType>& other) -> OffsetPtr&
{
    std::tie(offset_, memory_bounds_) = CopyFrom<OtherPointedType>(other, *this);
    return *this;
}

template <typename PointedType>
auto OffsetPtr<PointedType>::CalculateOffsetFromPointer(const void* const offset_ptr_address,
                                                        pointer pointed_to_address) noexcept -> difference_type
{
    if (pointed_to_address == nullptr)
    {
        return detail_offset_ptr::kNullPtrRepresentation;
    }
    const difference_type offset = SubtractPointersBytes(pointed_to_address, offset_ptr_address);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offset != detail_offset_ptr::kNullPtrRepresentation,
                           "Calculated offset must not equal the null representation.");
    return offset;
}

template <typename PointedType>
// Suppress "AUTOSAR C++14 A0-1-3" rule finding. This rule states: "Every function defined in an anonymous
// namespace, or static function with internal linkage, or private member function shall be used.".
// Rationale: False-positive - method is used in this file.
// coverity[autosar_cpp14_a0_1_3_violation : FALSE]
auto OffsetPtr<PointedType>::CalculatePointerFromOffset(const difference_type offset,
                                                        const void* const offset_ptr) noexcept -> pointer
{
    if (offset == detail_offset_ptr::kNullPtrRepresentation)
    {
        return nullptr;
    }

    // Suppress AUTOSAR C++14 M6-4-1 rule finding. This rule states: "An if ( condition ) construct shall be followed
    // by a compound statement. The else keyword shall be followed by either a compound statement, or another if
    // statement.".
    // Suppress "AUTOSAR C++14 A7-1-8" rule finding. This rule states: "A class, structure, or enumeration shall not
    // be declared in the definition of its type.".
    // Rationale: These are false positives: "if constexpr" is a valid statement since C++17 which coverity apparently
    // does not properly handle.
    // coverity[autosar_cpp14_m6_4_1_violation]
    // coverity[autosar_cpp14_a7_1_8_violation]
    if constexpr (std::is_const_v<PointedType>)
    {
        // NOLINTNEXTLINE(score-banned-function) See justification above class.
        return static_cast<pointer>(AddOffsetToPointer<const void>(offset_ptr, offset));
    }
    else
    {

        // NOLINTNEXTLINE(score-banned-function) See justification above class.
        return static_cast<pointer>(AddOffsetToPointer<void>(offset_ptr, offset));
    }
}

template <typename PointedType>
template <typename OtherPointedType>
auto OffsetPtr<PointedType>::CopyFrom(const OffsetPtr<OtherPointedType>& source_offset_ptr,
                                      OffsetPtr<PointedType>& target_offset_ptr)
    -> std::pair<difference_type, MemoryRegionBounds>
{
    // memory_bounds is empty by default unless bounds checks is enabled and the OffsetPtr is being copied to the stack
    MemoryRegionBounds memory_bounds{};

    if (detail_offset_ptr::IsBoundsCheckingEnabled())
    {
        const auto source_offset_ptr_bounds =
            MemoryResourceRegistry::getInstance().GetBoundsFromAddress(&source_offset_ptr);
        const auto target_offset_ptr_bounds =
            MemoryResourceRegistry::getInstance().GetBoundsFromAddress(&target_offset_ptr);

        const bool copying_from_shm_to_stack =
            source_offset_ptr_bounds.has_value() && !target_offset_ptr_bounds.has_value();
        const bool copying_from_stack_to_stack =
            !source_offset_ptr_bounds.has_value() && !target_offset_ptr_bounds.has_value();
        if (copying_from_shm_to_stack)
        {
            memory_bounds = source_offset_ptr_bounds.value();
        }
        else if (copying_from_stack_to_stack)
        {
            memory_bounds = source_offset_ptr.memory_bounds_;
        }
    }

    auto* const other_pointed_to_object =
        OffsetPtr<OtherPointedType>::GetPointerWithoutBoundsCheck(&source_offset_ptr, source_offset_ptr.offset_);
    if (other_pointed_to_object == nullptr)
    {
        return {detail_offset_ptr::kNullPtrRepresentation, {}};
    }
    auto* const pointed_to_object = static_cast<PointedType*>(other_pointed_to_object);
    const auto offset = CalculateOffsetFromPointer(&target_offset_ptr, pointed_to_object);
    return {offset, memory_bounds};
}

template <typename PointedType>
// coverity[autosar_cpp14_a0_1_3_violation : FALSE] false-positive: Function is used in this file.
auto OffsetPtr<PointedType>::GetPointerWithBoundsCheck(
    const void* const offset_ptr_address,
    const detail_offset_ptr::difference_type offset,
    const MemoryRegionBounds& offset_ptr_memory_bounds_when_not_in_shm,
    const std::size_t pointed_type_size) -> pointer
{
    if (detail_offset_ptr::IsBoundsCheckingEnabled())
    {
        const auto offset_ptr_bounds = MemoryResourceRegistry::getInstance().GetBoundsFromAddress(offset_ptr_address);
        const auto is_offset_ptr_in_memory_region = offset_ptr_bounds.has_value();
        if (is_offset_ptr_in_memory_region)
        {
            // We use SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD instead of std::terminate so that we can check these in unit tests using
            // SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED instead of death tests (since death tests are very slow).
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(DoesOffsetPtrInSharedMemoryPassBoundsChecks(offset_ptr_address,
                                                                       offset,
                                                                       offset_ptr_bounds.value(),
                                                                       pointed_type_size,
                                                                       sizeof(OffsetPtr<PointedType>)));
        }
        else
        {
            // We use SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD instead of std::terminate so that we can check these in unit tests using
            // SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED instead of death tests (since death tests are very slow).
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(DoesOffsetPtrNotInSharedMemoryPassBoundsChecks(offset_ptr_address,
                                                                          offset,
                                                                          offset_ptr_memory_bounds_when_not_in_shm,
                                                                          pointed_type_size,
                                                                          sizeof(OffsetPtr<PointedType>)));
        }
    }

    // NOLINTNEXTLINE(score-banned-function) The current function is banned for calling this function
    return GetPointerWithoutBoundsCheck(offset_ptr_address, offset);
}

template <typename PointedType>
auto OffsetPtr<PointedType>::GetPointerWithoutBoundsCheck(const void* const offset_ptr_address,
                                                          const detail_offset_ptr::difference_type offset) noexcept
    -> pointer
{
    // NOLINTNEXTLINE(score-banned-function) The current function is banned for calling this function
    return CalculatePointerFromOffset(offset, offset_ptr_address);
}

// Enabled if PointedType is not void
template <typename PointedType>
template <class T, class>
auto OffsetPtr<PointedType>::pointer_to(std::add_lvalue_reference_t<T> ref) noexcept -> OffsetPtr<PointedType>
{
    return OffsetPtr(&ref);
}

// Enabled if PointedType is void
template <typename PointedType>
template <class T, class>
auto OffsetPtr<PointedType>::pointer_to(void* const r) noexcept -> OffsetPtr<PointedType>
{
    return OffsetPtr<void>(r);
}

// Enabled if PointedType is void
template <typename PointedType>
template <class T, class>
auto OffsetPtr<PointedType>::pointer_to(const void* const r) noexcept -> OffsetPtr<PointedType>
{
    return OffsetPtr<const void>(r);
}

template <typename PointedType>
auto OffsetPtr<PointedType>::operator*() const -> reference
{
    const pointer ptr = get();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(ptr != nullptr, "Cannot dereference a nullptr.");
    reference ref = *ptr;
    return ref;
}

template <typename PointedType>
auto OffsetPtr<PointedType>::operator[](difference_type idx) const -> reference
{
    // NOLINTNEXTLINE(score-banned-function) See justification above class.
    pointer ptr = GetPointerWithBoundsCheck(
        this, offset_ + (static_cast<difference_type>(sizeof(PointedType)) * idx), memory_bounds_, sizeof(PointedType));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(ptr != nullptr, "Cannot dereference a nullptr.");
    return *ptr;
}

template <typename PointedType>
// coverity[autosar_cpp14_a0_1_3_violation : FALSE] false-positive: Function is used in this file.
void OffsetPtr<PointedType>::IncrementOffset(const std::size_t multiple_of_pointed_type_to_increment)
{
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Rationale: False positive - There is no variable defined in the following line or inside the macro.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        multiple_of_pointed_type_to_increment <= (std::numeric_limits<std::size_t>::max() / sizeof(PointedType)),
        "Calculating number of bytes to increment would overflow std::size_t");
    const auto number_of_bytes_to_increment = multiple_of_pointed_type_to_increment * sizeof(PointedType);

    offset_ = AddUnsignedToSigned(offset_, number_of_bytes_to_increment);
}

template <typename PointedType>
// coverity[autosar_cpp14_a0_1_3_violation : FALSE] false-positive: Function is used in this file.
void OffsetPtr<PointedType>::DecrementOffset(const std::size_t multiple_of_pointed_type_to_decrement)
{
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Rationale: False positive - There is no variable defined in the following line or inside the macro.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        multiple_of_pointed_type_to_decrement <= (std::numeric_limits<std::size_t>::max() / sizeof(PointedType)),
        "Calculating number of bytes to decrement would overflow std::size_t");
    const auto number_of_bytes_to_decrement = multiple_of_pointed_type_to_decrement * sizeof(PointedType);

    offset_ = SubtractUnsignedFromSigned(offset_, number_of_bytes_to_decrement);
}

template <typename PointedType>
auto OffsetPtr<PointedType>::operator+=(difference_type offset) -> OffsetPtr&
{
    if (offset > 0)
    {
        IncrementOffset(static_cast<std::size_t>(offset));
    }
    else if (offset < 0)
    {
        DecrementOffset(static_cast<std::size_t>(AbsoluteValue(offset)));
    }
    return *this;
}

template <typename PointedType>
auto OffsetPtr<PointedType>::operator-=(difference_type offset) -> OffsetPtr&
{
    if (offset > 0)
    {
        DecrementOffset(static_cast<std::size_t>(offset));
    }
    else if (offset < 0)
    {
        IncrementOffset(static_cast<std::size_t>(AbsoluteValue(offset)));
    }
    return *this;
}

template <typename PointedType>
auto OffsetPtr<PointedType>::operator++() -> OffsetPtr&
{
    IncrementOffset(1U);
    return *this;
}

template <typename PointedType>
auto OffsetPtr<PointedType>::operator++(std::int32_t) -> OffsetPtr
{
    const OffsetPtr<PointedType> tmp(*this);
    IncrementOffset(1U);
    return tmp;
}

template <typename PointedType>
auto OffsetPtr<PointedType>::operator--() -> OffsetPtr&
{
    DecrementOffset(1U);
    return *this;
}

template <typename PointedType>
auto OffsetPtr<PointedType>::operator--(std::int32_t) -> OffsetPtr
{
    const OffsetPtr<PointedType> tmp(*this);
    DecrementOffset(1U);
    return tmp;
}

// We generate operators for comparing OffsetPtrs with other OffsetPtrs of the same type or pointers of the same type
// with all combinations of (non-) const / volatile.
template <typename T1, typename T2, class = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: Function is declared more than once.
// Rationale: Overload signatures are different
// Suppress "AUTOSAR C++14 A13-5-5", The rule states: "Comparison operators shall be non-member functions with identical
// parameter types and noexcept.".
// Rationale: The justification for the rule is that operators with non-identical operators can be
// confusing. Since there is no functional reason behind and we require comparing an OffsetPtr with a regular ptr, we
// tolerate this deviation.
// Suppress "AUTOSAR C++14 A7-1-8", The rule states: "A non-type specifier shall be placed before a type specifier in a
// declaration.".
// Rationale: False positive: all non-type specifiers are before any type specifiers.
// coverity[autosar_cpp14_a13_5_5_violation]
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
// coverity[autosar_cpp14_a7_1_8_violation : FALSE]
inline bool operator==(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2)
{
    return OffsetPtr<T1>::GetPointerWithoutBoundsCheck(&offset_ptr1, offset_ptr1.offset_) ==
           OffsetPtr<T2>::GetPointerWithoutBoundsCheck(&offset_ptr2, offset_ptr2.offset_);
}

template <typename T1, typename T2, class = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator==(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2)
{
    return OffsetPtr<T1>::GetPointerWithoutBoundsCheck(&offset_ptr1, offset_ptr1.offset_) == ptr2;
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator==(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2)
{
    return ptr1 == OffsetPtr<T2>::GetPointerWithoutBoundsCheck(&offset_ptr2, offset_ptr2.offset_);
}

template <typename T>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
inline bool operator==(const OffsetPtr<T>& offset_ptr1, const std::nullptr_t /*null_ptr2*/) noexcept
{
    return !static_cast<bool>(offset_ptr1);
}

template <typename T>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator==(const std::nullptr_t /*null_ptr1*/, const OffsetPtr<T>& offset_ptr2) noexcept
{
    return !static_cast<bool>(offset_ptr2);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator!=(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2) noexcept
{
    return !(offset_ptr1 == offset_ptr2);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator!=(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2) noexcept
{
    return !(offset_ptr1 == ptr2);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator!=(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2) noexcept
{
    return !(ptr1 == offset_ptr2);
}

template <typename T>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
inline bool operator!=(const OffsetPtr<T>& offset_ptr1, const std::nullptr_t null_ptr2) noexcept
{
    return !(offset_ptr1 == null_ptr2);
}

template <typename T>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
inline bool operator!=(const std::nullptr_t null_ptr1, const OffsetPtr<T>& offset_ptr2) noexcept
{
    return !(null_ptr1 == offset_ptr2);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator<(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2)
{
    return OffsetPtr<T1>::GetPointerWithoutBoundsCheck(&offset_ptr1, offset_ptr1.offset_) <
           OffsetPtr<T2>::GetPointerWithoutBoundsCheck(&offset_ptr2, offset_ptr2.offset_);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator<(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2)
{
    // Logic of making comparison operator considers utilizing internal pointers.
    // NOLINTNEXTLINE(score-no-pointer-comparison): Tolerated pointer comparison
    return OffsetPtr<T1>::GetPointerWithoutBoundsCheck(&offset_ptr1, offset_ptr1.offset_) < ptr2;
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator<(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2)
{
    return ptr1 < OffsetPtr<T2>::GetPointerWithoutBoundsCheck(&offset_ptr2, offset_ptr2.offset_);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator<=(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2)
{
    return OffsetPtr<T1>::GetPointerWithoutBoundsCheck(&offset_ptr1, offset_ptr1.offset_) <=
           OffsetPtr<T2>::GetPointerWithoutBoundsCheck(&offset_ptr2, offset_ptr2.offset_);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator<=(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2)
{
    // Logic of making comparison operator considers utilizing internal pointers.
    // NOLINTNEXTLINE(score-no-pointer-comparison): Tolerated pointer comparison
    return OffsetPtr<T1>::GetPointerWithoutBoundsCheck(&offset_ptr1, offset_ptr1.offset_) <= ptr2;
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator<=(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2)
{
    return ptr1 <= OffsetPtr<T2>::GetPointerWithoutBoundsCheck(&offset_ptr2, offset_ptr2.offset_);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator>(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2) noexcept
{
    return !(offset_ptr1 <= offset_ptr2);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator>(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2) noexcept
{
    return !(offset_ptr1 <= ptr2);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator>(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2) noexcept
{
    return !(ptr1 <= offset_ptr2);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator>=(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2) noexcept
{
    return !(offset_ptr1 < offset_ptr2);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator>=(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2) noexcept
{
    return !(offset_ptr1 < ptr2);
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a13_5_5_violation] See rationale for autosar_cpp14_a13_5_5_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline bool operator>=(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2) noexcept
{
    return !(ptr1 < offset_ptr2);
}

template <typename T>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
inline auto operator+(typename OffsetPtr<T>::difference_type diff, OffsetPtr<T> right) noexcept -> OffsetPtr<T>
{
    right += diff;
    return right;
}

template <typename T>
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: Function is declared more than once.
// Rationale: The two overloads of operator+ are necessary to support both commutative forms of addition
// between OffsetPtr<T> and its difference_type. This is a valid use of function overloading in C++, and
// the warning is a false positive because the rule is intended to prevent identical declarations, not
// legitimate overloads.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
inline auto operator+(OffsetPtr<T> left, typename OffsetPtr<T>::difference_type diff) -> OffsetPtr<T>
{
    left += diff;
    return left;
}

template <typename T>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
inline auto operator-(OffsetPtr<T> left, typename OffsetPtr<T>::difference_type diff) -> OffsetPtr<T>
{
    left -= diff;
    return left;
}

template <typename T>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
inline auto operator-(typename OffsetPtr<T>::difference_type diff, OffsetPtr<T> right) -> OffsetPtr<T>
{
    right -= diff;
    return right;
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline auto operator-(const OffsetPtr<T1>& offset_ptr1, const OffsetPtr<T2>& offset_ptr2) ->
    typename OffsetPtr<T1>::difference_type
{
    auto* const ptr1 = OffsetPtr<T1>::GetPointerWithoutBoundsCheck(&offset_ptr1, offset_ptr1.offset_);
    auto* const ptr2 = OffsetPtr<T2>::GetPointerWithoutBoundsCheck(&offset_ptr2, offset_ptr2.offset_);
    using diff_t = typename OffsetPtr<T1>::difference_type;
    const auto offset_result = safe_math::Divide<diff_t>(SubtractPointersBytes(ptr1, ptr2), sizeof(T1));

    // OffsetPtr subtraction should only be done when both OffsetPtrs point to elements of the same array (we follow
    // the rules of regular pointer arithmetic: https://timsong-cpp.github.io/cppwp/n4659/expr.add#5). In these
    // cases, the addresses of all elements in the array must be multiples of sizeof(T1)
    // (https://timsong-cpp.github.io/cppwp/std17/dcl.array#1) and therefore the difference between 2 the addresses
    // of two elements of the same array must be a multiple of sizeof(T1). Therefore, this branch will only be
    // entered if offset_ptr1 and offset_ptr2 don't point to elements of the same array. This violates a
    // precondition of this function so we terminate.
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE((offset_result.has_value()),
                           "Different between OffsetPtr addresses is not multiple of PointedType size.");
    return offset_result.value();
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline auto operator-(const OffsetPtr<T1>& offset_ptr1, T2* const ptr2) -> typename OffsetPtr<T1>::difference_type
{
    auto* const ptr1 = OffsetPtr<T1>::GetPointerWithoutBoundsCheck(&offset_ptr1, offset_ptr1.offset_);
    using diff_t = typename OffsetPtr<T1>::difference_type;
    const auto offset_result = safe_math::Divide<diff_t>(SubtractPointersBytes(ptr1, ptr2), sizeof(T1));

    // OffsetPtr subtraction should only be done when both pointers point to elements of the same array (we follow
    // the rules of regular pointer arithmetic: https://timsong-cpp.github.io/cppwp/n4659/expr.add#5). In these
    // cases, the addresses of all elements in the array must be multiples of sizeof(T1)
    // (https://timsong-cpp.github.io/cppwp/std17/dcl.array#1) and therefore the difference between 2 the addresses
    // of two elements of the same array must be a multiple of sizeof(T1). Therefore, this branch will only be
    // entered if offset_ptr1 and offset_ptr2 don't point to elements of the same array. This violates a
    // precondition of this function so we terminate.
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE((offset_result.has_value()),
                           "Different between OffsetPtr and ptr addresses is not multiple of PointedType size.");
    return offset_result.value();
}

template <typename T1,
          typename T2,
          typename = std::enable_if_t<detail_offset_ptr::cv_stripped_types_equal<T1, T2>::value>>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
// coverity[autosar_cpp14_a7_1_8_violation : FALSE] See rationale for autosar_cpp14_a7_1_8_violation above
inline auto operator-(T1* const ptr1, const OffsetPtr<T2>& offset_ptr2) -> typename OffsetPtr<T1>::difference_type
{
    auto* const ptr2 = OffsetPtr<T2>::GetPointerWithoutBoundsCheck(&offset_ptr2, offset_ptr2.offset_);
    using diff_t = typename OffsetPtr<T1>::difference_type;
    const auto offset_result = safe_math::Divide<diff_t>(SubtractPointersBytes(ptr1, ptr2), sizeof(T1));

    // OffsetPtr subtraction should only be done when both pointers point to elements of the same array (we follow
    // the rules of regular pointer arithmetic: https://timsong-cpp.github.io/cppwp/n4659/expr.add#5). In these
    // cases, the addresses of all elements in the array must be multiples of sizeof(T1)
    // (https://timsong-cpp.github.io/cppwp/std17/dcl.array#1) and therefore the difference between 2 the addresses
    // of two elements of the same array must be a multiple of sizeof(T1). Therefore, this branch will only be
    // entered if offset_ptr1 and offset_ptr2 don't point to elements of the same array. This violates a
    // precondition of this function so we terminate.
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE((offset_result.has_value()),
                           "Different between OffsetPtr and ptr addresses is not multiple of PointedType size.");
    return offset_result.value();
}

template <typename T>
// coverity[autosar_cpp14_m3_2_3_violation : FALSE] See rationale for autosar_cpp14_m3_2_3_violation above
void swap(OffsetPtr<T>& left, OffsetPtr<T>& right)
{
    auto* const right_ptr = OffsetPtr<T>::GetPointerWithoutBoundsCheck(&right, right.offset_);
    right = left;
    left = right_ptr;
}

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_OFFSET_PTR_H
