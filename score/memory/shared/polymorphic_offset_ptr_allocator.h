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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_POLYMORPHICOFFSETPTRALLOCATOR_H
#define BASELIBS_SCORE_MEMORY_SHARED_POLYMORPHICOFFSETPTRALLOCATOR_H

#include "score/language/safecpp/safe_math/details/multiplication/multiplication.h"
#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/offset_ptr.h"

#include "score/language/safecpp/safe_math/safe_math.h"

#include <cstdint>
#include <cstdlib>
#include <type_traits>

namespace score::memory::shared
{

template <typename T = std::uint8_t>
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// this is false positive. PolymorphicOffsetPtrAllocator is declared only once.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
class PolymorphicOffsetPtrAllocator
{
  public:
    using value_type = T;
    using pointer = OffsetPtr<T>;
    using size_type = std::size_t;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;

    PolymorphicOffsetPtrAllocator() = default;
    ~PolymorphicOffsetPtrAllocator() = default;

    // Non-explicit constructor is good enough for maintaining required implicit conversion
    // NOLINTNEXTLINE(google-explicit-constructor): Tolerated, discard explicit.
    PolymorphicOffsetPtrAllocator(const MemoryResourceProxy* const proxy) noexcept : proxy_{proxy} {}

    template <typename U>
    // Non-explicit constructor is good enough for maintaining required implicit conversion.
    // In addition semantically is a copy constructor.
    // NOLINTNEXTLINE(google-explicit-constructor): Tolerated, discard explicit.
    PolymorphicOffsetPtrAllocator(const PolymorphicOffsetPtrAllocator<U>& rhs) noexcept
        : proxy_(rhs.getMemoryResourceProxy())
    {
    }

    PolymorphicOffsetPtrAllocator(const PolymorphicOffsetPtrAllocator&) = default;
    PolymorphicOffsetPtrAllocator(PolymorphicOffsetPtrAllocator&&) noexcept = default;
    PolymorphicOffsetPtrAllocator& operator=(const PolymorphicOffsetPtrAllocator&) = default;
    PolymorphicOffsetPtrAllocator& operator=(PolymorphicOffsetPtrAllocator&&) noexcept = default;

    auto allocate(size_type size) -> pointer;
    void deallocate(pointer p, size_type size);

    OffsetPtr<const MemoryResourceProxy> getMemoryResourceProxy() const noexcept;

    template <typename U>
    friend bool operator==(const PolymorphicOffsetPtrAllocator<U>& lhs,
                           const PolymorphicOffsetPtrAllocator<U>& rhs) noexcept;

    template <typename U>
    friend bool operator!=(const PolymorphicOffsetPtrAllocator<U>& lhs,
                           const PolymorphicOffsetPtrAllocator<U>& rhs) noexcept;

  private:
    OffsetPtr<const MemoryResourceProxy> proxy_;
};

template <typename T>
auto PolymorphicOffsetPtrAllocator<T>::allocate(size_type size) -> pointer
{
    void* allocatedMemory{nullptr};
    const auto number_bytes_to_allocate = safe_math::Multiply(size, sizeof(T)).value();
    if (proxy_ != nullptr)
    {
        // Suppress "AUTOSAR C++14 A5-3-2" finding rule. This rule states: "Null pointers shall not be
        // dereferenced.".
        // We check that proxy_ is not a nullptr in the previous line.
        // coverity[autosar_cpp14_a5_3_2_violation: FALSE]
        allocatedMemory = proxy_->allocate(number_bytes_to_allocate, alignof(T));
    }
    else
    {
        // Suppress "AUTOSAR C++14 A18-5-1" rule finding: Functions malloc, calloc, realloc and free shall not be
        // used.
        // Rationale : malloc needed for custom allocator
        // NOLINTBEGIN(cppcoreguidelines-no-malloc): malloc needed for custom allocator
        // coverity[autosar_cpp14_a18_5_1_violation]
        allocatedMemory = std::malloc(number_bytes_to_allocate);
        // NOLINTEND(cppcoreguidelines-no-malloc): see above
    }

    // Suppress "AUTOSAR C++14 M5-2-8" rule finding:  An object with integer type or pointer to void type shall not
    // be converted to an object with pointer type.
    // Rationale : fresh allocated memory needs to be converted to the type that shall be stored in it
    // coverity[autosar_cpp14_m5_2_8_violation]
    return pointer{static_cast<value_type*>(allocatedMemory)};
}

template <typename T>
void PolymorphicOffsetPtrAllocator<T>::deallocate(pointer p, size_type size)
{
    if (proxy_ != nullptr)
    {
        const auto number_bytes_to_deallocate = safe_math::Multiply(size, sizeof(T)).value();
        // Suppress "AUTOSAR C++14 A5-3-2" finding rule. This rule states: "Null pointers shall not be
        // dereferenced.".
        // We check that proxy_ is not a nullptr in the previous line.
        // coverity[autosar_cpp14_a5_3_2_violation: FALSE]
        proxy_->deallocate(static_cast<void*>(&*p), number_bytes_to_deallocate);
    }
    else
    {
        // Suppress "AUTOSAR C++14 A18-5-1" rule finding: Functions malloc, calloc, realloc and free shall not be
        // used.
        // Rationale : needed for custom allocator
        // NOLINTBEGIN(cppcoreguidelines-no-malloc): needed for custom allocator
        // coverity[autosar_cpp14_a18_5_1_violation]
        std::free(static_cast<void*>(&*p));
        // NOLINTEND(cppcoreguidelines-no-malloc): see above
    }
}

template <typename T>
// "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
// implicitly" Rationale: The OffsetPtr's copy constructor will be called in this function which is not noexcept.
// However, it is marked not noexcept to allow using exceptions during testing only (using the
// SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED macro) and will NEVER throw an exception in production code. Therefore, an implicit
// termination due to an exception being thrown will never occur here in production.
// coverity[autosar_cpp14_a15_5_3_violation]
OffsetPtr<const MemoryResourceProxy> PolymorphicOffsetPtrAllocator<T>::getMemoryResourceProxy() const noexcept
{
    return proxy_;
}

template <typename T>
bool operator==(const PolymorphicOffsetPtrAllocator<T>& lhs, const PolymorphicOffsetPtrAllocator<T>& rhs) noexcept
{
    if ((lhs.proxy_ == nullptr) && (rhs.proxy_ == nullptr))
    {
        return true;
    }
    if (lhs.proxy_ == nullptr)
    {
        return false;
    }
    if (rhs.proxy_ == nullptr)
    {
        return false;
    }
    return *(lhs.proxy_) == *(rhs.proxy_);
}

template <typename T>
bool operator!=(const PolymorphicOffsetPtrAllocator<T>& lhs, const PolymorphicOffsetPtrAllocator<T>& rhs) noexcept
{
    return !operator==(lhs, rhs);
}

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_POLYMORPHICOFFSETPTRALLOCATOR_H
