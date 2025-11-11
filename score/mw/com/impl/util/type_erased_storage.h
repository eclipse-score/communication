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
#ifndef SCORE_MW_COM_IMPL_UTIL_TYPE_ERASED_STORAGE_H
#define SCORE_MW_COM_IMPL_UTIL_TYPE_ERASED_STORAGE_H

#include "score/memory/shared/pointer_arithmetic_util.h"
#include <score/assert.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>

namespace score::mw::com::impl
{

struct TypeErasedDataTypeInfo
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types
    // shall be private.". This struct is a POD class!
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::size_t size{};
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::size_t alignment{};
};

bool operator==(const TypeErasedDataTypeInfo& lhs, const TypeErasedDataTypeInfo& rhs) noexcept;

// Suppress "AUTOSAR C++14 A11-0-2" rule finding.
// This rule states: "A type defined as struct shall: (1) provide only public data members, (2)
// not provide any special member functions or methods, (3) not be a base of
// another struct or class, (4) not inherit from another struct or class.".
// Rationale: No point of this rule is violated. We are providing a specific ctor, not the default ctor!
// coverity[autosar_cpp14_a11_0_2_violation]
struct MemoryBufferAccessor
{
    MemoryBufferAccessor(std::byte* buffer, std::size_t size) : start{buffer}, offset{0U}, buffer_size{size} {}
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types
    // shall be private.". This struct is a POD class!
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::byte* start;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::size_t offset;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::size_t buffer_size;
};

namespace detail
{

/// \brief Aggregates new argument type into given TypeErasedDataTypeInfo.
/// \details Existing TypeErasedDataTypeInfo is virtually a "struct", which has a given size/alignment. This template
/// func semantically adds the given argument type as a new member, taking into account any needed padding and then
/// updates the overall size/alignment of the TypeErasedDataTypeInfo
/// \tparam Arg type of argument to be aggregated into existing TypeErasedDataTypeInfo
/// \param info existing TypeErasedDataTypeInfo
template <typename Arg>
constexpr void AggregateArgType(TypeErasedDataTypeInfo& info)
{
    auto padding = (info.size % alignof(Arg)) == 0 ? 0 : alignof(Arg) - (info.size % alignof(Arg));
    info.size += sizeof(Arg) + padding;
    info.alignment = std::max(info.alignment, alignof(Arg));
}

}  // namespace detail

/// \brief Creates meta-info (sizeof/alignment) a "type-erased representation" of the given argument types would have.
/// \details When we do a "type-erased" storage of the given arguments, we technically "simulate", to aggregate the
/// given argument types into a struct. The returned TypeErasedDataTypeInfo then contains the size and alignment the
/// hypothetical struct aggregating the given argument types, would have.
/// Example: Given the following Args: std::uint8_t, boolean, std::uint64_t, this function internally builds up the
/// following representation:
/// struct{
///    std::uint8_t a;
///    boolean b;
///    std::uint64_t c;
/// }
/// and then returns its sizeof/alignof.
/// \tparam Args argument types
/// \return  TypeErasedDataTypeInfo containing sizeof/alignof of the aggregated representation.
template <typename... Args>
constexpr TypeErasedDataTypeInfo CreateTypeErasedDataTypeInfoFromTypes()
{
    TypeErasedDataTypeInfo result{};

    ((detail::AggregateArgType<Args>(result)), ...);

    // final step: Adjust size to be a multiple of its alignment!
    // See: https://en.cppreference.com/w/cpp/language/sizeof.html -> "When applied to a class type, the result is the
    // number of bytes occupied by a complete object of that class, including any additional padding required to place
    // such object in an array."
    if (result.size % result.alignment != 0)
    {
        result.size += result.alignment - (result.size % result.alignment);
    }
    return result;
}

/// \brief Creates meta-info (sizeof/alignment) a type-erased representation of the given arguments would have.
/// \details See #CreateTypeErasedDataTypeInfoFromTypes(). This is a variation, which takes parameters of the argument
/// types, which is "easier" in some call-contexts for template argument deduction.
/// \tparam Args argument types
/// \param ... values for the argument types, just used for type deduction.
/// \return TypeErasedDataTypeInfo containing sizeof/alignof of the aggregated representation.
template <typename... Args>
constexpr TypeErasedDataTypeInfo CreateTypeErasedDataTypeInfoFromValues(Args...)
{
    return CreateTypeErasedDataTypeInfoFromTypes<Args...>();
}

/// \brief template to serialize argument value into given buffer.
/// \details This is the base case for #SerializeArgs(MemoryBuffer&, T, Args...). Further details see there.
/// \tparam T current argument type
/// \param target_buffer where to serialize to.
/// \param arg argument value to serialize.
template <typename T>
void SerializeArgs(MemoryBufferAccessor& target_buffer, T arg)
{
    auto dest_ptr =
        static_cast<void*>(score::memory::shared::AddOffsetToPointer(target_buffer.start, target_buffer.offset));
    const std::size_t buffer_space_before_align = target_buffer.buffer_size - target_buffer.offset;
    std::size_t buffer_space_after_align = buffer_space_before_align;
    dest_ptr = std::align(alignof(T), sizeof(T), dest_ptr, buffer_space_after_align);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(dest_ptr, "Buffer too small");
    std::memcpy(dest_ptr, &arg, sizeof(T));

    const auto additional_buffer_used = buffer_space_before_align - buffer_space_after_align;
    target_buffer.offset += sizeof(T) + additional_buffer_used;
}

/// \brief Variadic template to serialize argument values into given buffer.
/// \details Typical usage of this func template is to serialize strongly typed arguments into a type erased storage
/// (the target_buffer). So the regular steps are: 1st call one of the CreateTypeErasedDataTypeInfoFromXXX() functions
/// on the arguments/argument types, you want to store "type-erased". This gives you then a TypeErasedDataTypeInfo,
/// which contain sizeof/alignof needs of the type-erased storage for the given arguments. 2nd step is to allocate a
/// memory buffer with the alignment and size of TypeErasedDataTypeInfo. Then in the last step you call this func, to
/// serialize the typed arguments into the buffer. I.e. CreateTypeErasedDataTypeInfoFromXXX/SerializeArgs form a pair
/// of funcs, which both use/expect the same "storage format" for a given sequence of argument types.
/// "Serialization" here means doing a mem-copy, taking into account alignment needs (do the needed padding)
/// \tparam T front/current argument type
/// \tparam Args remaining argument types
/// \param target_buffer buffer, whee to serialize to. Typically the buffer (start) should be already worst case
/// aligned.
///        If it isn't, depending on the alignment needs of the arguments, it can happen, that the initial bytes need to
///        be used as padding bytes.
/// \param arg Front/current argument value
/// \param args Remaining argument values.
/// \remarks Will assert/terminate, if buffer isn't sized large enough.
template <typename T, typename... Args>
void SerializeArgs(MemoryBufferAccessor& target_buffer, T arg, Args... args)
{
    SerializeArgs(target_buffer, arg);
    SerializeArgs(target_buffer, args...);
}

/// \brief Returns pointer to argument value at current buffer position.
/// \details Casts the current buffer position (with eventually added padding bytes if needed) to a pointer to Arg type
/// and returns it. Updates the buffer,offset_ to point after the argument
/// \tparam Arg type of argument
/// \param buffer buffer containing argument value at the current offset
/// \return pointer to argument of given type.
template <typename Arg>
auto DeserializeArg(MemoryBufferAccessor& buffer) -> Arg*
{
    auto src_ptr = static_cast<void*>(score::memory::shared::AddOffsetToPointer(buffer.start, buffer.offset));
    auto src_ptr_as_int = score::memory::shared::CastPointerToInteger(src_ptr);

    auto padding = (src_ptr_as_int % alignof(Arg)) == 0 ? 0 : alignof(Arg) - (src_ptr_as_int % alignof(Arg));
    buffer.offset += (padding + sizeof(Arg));
    return static_cast<Arg*>(score::memory::shared::AddOffsetToPointer(src_ptr, padding));
}

/// \brief Takes a type-erased storage of the given argument types in the form of a memory buffer and returns pointers
/// to (strongly typed) args within the storage.
/// \remark Expects, that the "type-erased" storage within src_buffer has been created via
/// #SerializeArgs(MemoryBuffer&, Args...)
/// \tparam Args Expected types within "type erased" storage.
/// \param src_buffer memory buffer containing the "type erased" storage.
/// \return Tuple of pointers to the arguments.
template <typename... Args>
auto Deserialize(MemoryBufferAccessor& src_buffer) -> std::tuple<typename std::add_pointer<Args>::type...>
{
    auto tuple_with_args = std::tuple<typename std::add_pointer<Args>::type...>();
    std::apply(
        [&src_buffer](auto&&... args) noexcept {
            ((args =
                  DeserializeArg<typename std::remove_pointer_t<std::remove_reference_t<decltype(args)>>>(src_buffer)),
             ...);
        },
        tuple_with_args);
    return tuple_with_args;
}

}  // namespace score::mw::com::impl

#endif
