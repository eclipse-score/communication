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

#include "score/memory/data_type_size_info.h"

#include "score/memory/shared/pointer_arithmetic_util.h"

#include <score/assert.hpp>
#include <score/span.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <tuple>
#include <type_traits>

namespace score::mw::com::impl
{

namespace detail
{

// Suppress "AUTOSAR C++14 A11-0-2" rule finding.
// This rule states: "A type defined as struct shall: (1) provide only public data members, (2)
// not provide any special member functions or methods, (3) not be a base of
// another struct or class, (4) not inherit from another struct or class.".
// Rationale: No point of this rule is violated. We are providing a specific ctor, not the default ctor!
// coverity[autosar_cpp14_a11_0_2_violation]
class MemoryBufferAccessor
{
  public:
    explicit MemoryBufferAccessor(const score::cpp::span<std::byte> input_buffer) : buffer{input_buffer}, offset{0U} {}
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types
    // shall be private.". This struct is a POD class!
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::cpp::span<std::byte> buffer;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::size_t offset;
};

/// \brief Aggregates new argument type into given DataTypeSizeInfo.
/// \details Existing DataTypeSizeInfo is virtually a "struct", which has a given size/alignment. This template
/// func semantically adds the given argument type as a new member, taking into account any needed padding and then
/// updates the overall size/alignment of the DataTypeSizeInfo
/// \tparam Arg type of argument to be aggregated into existing DataTypeSizeInfo
/// \param info existing DataTypeSizeInfo
template <typename Arg>
constexpr void AggregateArgType(std::size_t& size, std::size_t& alignment)
{
    auto padding = (size % alignof(Arg)) == 0 ? 0 : alignof(Arg) - (size % alignof(Arg));
    size += sizeof(Arg) + padding;
    alignment = std::max(alignment, alignof(Arg));
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
    // NOLINTNEXTLINE(score-banned-function) TODO Ticket-228560
    auto src_ptr = static_cast<void*>(score::memory::shared::AddOffsetToPointer(buffer.buffer.data(), buffer.offset));
    auto src_ptr_as_int = score::memory::shared::CastPointerToInteger(src_ptr);

    auto padding = (src_ptr_as_int % alignof(Arg)) == 0 ? 0 : alignof(Arg) - (src_ptr_as_int % alignof(Arg));
    buffer.offset += (padding + sizeof(Arg));
    // NOLINTNEXTLINE(score-banned-function) TODO Ticket-228560
    return static_cast<Arg*>(score::memory::shared::AddOffsetToPointer(src_ptr, padding));
}

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

/// \brief template to serialize argument value into given buffer.
/// \details This is the base case for #SerializeArgs(MemoryBuffer&, T, Args...). Further details see there.
/// \tparam T current argument type
/// \param target_buffer where to serialize to.
/// \param arg argument value to serialize.
template <typename T>
void SerializeArgs(MemoryBufferAccessor& target_buffer, T& arg)
{
    auto* dest_ptr =
        // In our architecture we have a one-to-one mapping between pointers and integral values.
        // Therefore, casting between the two is well-defined.
        // The resulting pointer is used as start of the memory arena for the allocation.
        // In C++23 std::start_lifetime_as_array can be used to inform the compiler.
        // NOLINTNEXTLINE(score-banned-function) see above
        static_cast<void*>(score::memory::shared::AddOffsetToPointer(target_buffer.buffer.data(), target_buffer.offset));
    const std::size_t buffer_space_before_align = target_buffer.buffer.size() - target_buffer.offset;
    std::size_t buffer_space_after_align = buffer_space_before_align;
    dest_ptr = std::align(alignof(T), sizeof(T), dest_ptr, buffer_space_after_align);

    const auto AreRegionsOverlapping = [](MemoryBufferAccessor& destination_buffer, T source_object) {
        using memory::shared::CastPointerToInteger;
        using memory::shared::AddOffsetToPointerAsInteger;
        const auto target_buffer_start_address = CastPointerToInteger(destination_buffer.buffer.data());
        const auto target_buffer_end_address =
            AddOffsetToPointerAsInteger(target_buffer_start_address, destination_buffer.buffer.size());
        const auto arg_start_address = CastPointerToInteger(&source_object);
        const auto arg_end_address = AddOffsetToPointerAsInteger(arg_start_address, sizeof(source_object));

        const bool is_arg_before_target_buffer = arg_end_address < target_buffer_start_address;
        const bool is_arg_after_target_buffer = arg_start_address > target_buffer_end_address;
        return !(is_arg_before_target_buffer || is_arg_after_target_buffer);
    };
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(dest_ptr != nullptr, "Buffer too small");
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(!AreRegionsOverlapping(target_buffer, arg), "arg is already inside target_buffer!");
    static_assert(std::is_trivially_copyable_v<T>, "std::memcpy assumes that type T is trivially copyable!");
    // NOLINTBEGIN(score-banned-function): Since the output buffer is type erased, we must do a memcpy rather than an
    // explicit copy using the copy constructor of type T.
    // - We have a static assert that T is trivially copyable to ensure that there are no side effects of a copy
    // constructor which are missed by using memcpy.
    // - We have an assertion that dest_ptr is not a nullptr. The source pointer is the address of arg so cannot be a
    // nullptr.
    // - We ensure that dest_ptr is aligned with T above (in std::align).
    // - We ensure that arg will fit inside the destination buffer (in std::align).
    // - We ensure that the destination buffer and source object don't overlap (in AreRegionsOverlapping).
    std::memcpy(dest_ptr, &arg, sizeof(T));
    // NOLINTEND(score-banned-function)

    const auto additional_buffer_used = buffer_space_before_align - buffer_space_after_align;
    target_buffer.offset += sizeof(T) + additional_buffer_used;
}

template <typename T, typename... Args>
void SerializeArgs(MemoryBufferAccessor& target_buffer, T& arg, Args&... args)
{
    SerializeArgs(target_buffer, arg);
    SerializeArgs(target_buffer, args...);
}

}  // namespace detail

/// \brief Creates meta-info (sizeof/alignment) a "type-erased representation" of the given argument types would have.
/// \details When we do a "type-erased" storage of the given arguments, we technically "simulate", to aggregate the
/// given argument types into a struct. The returned DataTypeSizeInfo then contains the size and alignment the
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
/// \return  DataTypeSizeInfo containing sizeof/alignof of the aggregated representation.
template <typename... Args>
constexpr memory::DataTypeSizeInfo CreateDataTypeSizeInfoFromTypes()
{
    std::size_t total_size{0U};
    std::size_t total_alignment{0U};

    ((detail::AggregateArgType<Args>(total_size, total_alignment)), ...);

    // final step: Adjust size to be a multiple of its alignment!
    // See: https://en.cppreference.com/w/cpp/language/sizeof.html -> "When applied to a class type, the result is the
    // number of bytes occupied by a complete object of that class, including any additional padding required to place
    // such object in an array."
    if ((total_alignment != 0) && (total_size % total_alignment != 0))
    {
        total_size += total_alignment - (total_size % total_alignment);
    }
    return {total_size, total_alignment};
}

/// \brief Creates meta-info (sizeof/alignment) a type-erased representation of the given arguments would have.
/// \details See #CreateDataTypeSizeInfoFromTypes(). This is a variation, which takes parameters of the argument
/// types, which is "easier" in some call-contexts for template argument deduction.
/// \tparam Args argument types
/// \param ... values for the argument types, just used for type deduction.
/// \return DataTypeSizeInfo containing sizeof/alignof of the aggregated representation.
template <typename... Args>
constexpr memory::DataTypeSizeInfo CreateDataTypeSizeInfoFromValues(Args...)
{
    return CreateDataTypeSizeInfoFromTypes<Args...>();
}

/// \brief Variadic template to serialize argument values into given buffer.
/// \details Typical usage of this func template is to serialize strongly typed arguments into a type erased storage
/// (the target_buffer). So the regular steps are: 1st call one of the CreateDataTypeSizeInfoFromXXX() functions
/// on the arguments/argument types, you want to store "type-erased". This gives you then a DataTypeSizeInfo,
/// which contain sizeof/alignof needs of the type-erased storage for the given arguments. 2nd step is to allocate a
/// memory buffer with the alignment and size of DataTypeSizeInfo. Then in the last step you call this func, to
/// serialize the typed arguments into the buffer. I.e. CreateDataTypeSizeInfoFromXXX/SerializeArgs form a pair
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
void SerializeArgs(score::cpp::span<std::byte> target_buffer, T& arg, Args&... args)
{
    detail::MemoryBufferAccessor memory_buffer_accessor{target_buffer};
    detail::SerializeArgs(memory_buffer_accessor, arg, args...);
}

/// \brief Takes a type-erased storage of the given argument types in the form of a memory buffer and returns pointers
/// to (strongly typed) args within the storage.
/// \remark Expects, that the "type-erased" storage within src_buffer has been created via
/// #SerializeArgs(MemoryBuffer&, Args...)
/// \tparam Args Expected types within "type erased" storage.
/// \param src_buffer memory buffer containing the "type erased" storage.
/// \return Tuple of pointers to the arguments.
template <typename... Args>
auto Deserialize(score::cpp::span<std::byte> src_buffer) -> std::tuple<typename std::add_pointer<Args>::type...>
{
    detail::MemoryBufferAccessor memory_buffer_accessor{src_buffer};
    return detail::Deserialize<Args...>(memory_buffer_accessor);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_UTIL_TYPE_ERASED_STORAGE_H
