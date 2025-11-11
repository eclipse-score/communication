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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SAMPLE_PTR_H
#define SCORE_MW_COM_IMPL_PLUMBING_SAMPLE_PTR_H

#include "score/mw/com/impl/sample_reference_tracker.h"

#include "score/mw/com/impl/bindings/lola/sample_ptr.h"
#include "score/mw/com/impl/bindings/mock_binding/sample_ptr.h"

#include <score/blank.hpp>
#include <score/overload.hpp>

#include <utility>
#include <variant>

namespace score::mw::com::impl
{

/// \brief Binding agnostic reference to a sample received from a proxy event binding.
///
/// The class resembles std::unique_ptr but does not allocate. Instead, all pointer types from all supported bindings
/// need to be added to the variant so that this class can hold such an instance.
///
/// \tparam SampleType Data type referenced by this pointer.
template <typename SampleType>
class SamplePtr final
{
  public:
    using pointer = const SampleType*;
    using element_type = const SampleType;

    /// Create an instance by taking ownership of one of the supported inner sample pointer types.
    ///
    /// \tparam SamplePtrType The type of the sample pointer. Needs to be one of the types listed in the variant holding
    ///                       the actual pointer.
    /// \param binding_sample_ptr The binding-specific sample pointer.
    template <typename SamplePtrType>
    SamplePtr(SamplePtrType&& binding_sample_ptr, SampleReferenceGuard reference_guard)
        : binding_sample_ptr_{std::forward<SamplePtrType>(binding_sample_ptr)},
          reference_guard_{std::move(reference_guard)}
    {
    }

    // Suppress "AUTOSAR C++14 A13-3-1", The rule states: "A function that contains “forwarding reference” as its
    // argument shall not be overloaded".
    // As we here have a different number of parameters, so no way of confusion on which function will be called.
    // coverity[autosar_cpp14_a13_3_1_violation]
    constexpr SamplePtr() noexcept : SamplePtr{score::cpp::blank{}, SampleReferenceGuard{}} {}

    // NOLINTBEGIN(google-explicit-constructor): Explanation provided below.
    // Clang tidy deviation: Constructor is not made explicit to allow implicit creation of a SamplePtr from a nullptr.
    // This simplifies code in many places e.g. allowing the default argument for a SamplePtr function argument to be a
    // nullptr: void TestFunc(SamplePtr<T> ptr = nullptr)
    // coverity[autosar_cpp14_a12_1_4_violation] see above rationale for clang-tidy
    // coverity[autosar_cpp14_a13_3_1_violation]
    constexpr SamplePtr(std::nullptr_t /* ptr */) noexcept : SamplePtr() {}
    // NOLINTEND(google-explicit-constructor): see above for detailed explanation

    SamplePtr(const SamplePtr<SampleType>&) = delete;
    // coverity[autosar_cpp14_a13_3_1_violation]
    SamplePtr(SamplePtr<SampleType>&& other) noexcept
        : binding_sample_ptr_{std::move(other.binding_sample_ptr_)}, reference_guard_{std::move(other.reference_guard_)}
    {
        other.binding_sample_ptr_ = score::cpp::blank{};
    }
    SamplePtr& operator=(const SamplePtr<SampleType>&) = delete;
    SamplePtr& operator=(SamplePtr<SampleType>&& other) & noexcept = default;

    SamplePtr& operator=(std::nullptr_t) noexcept
    {
        Reset();
        return *this;
    }

    ~SamplePtr() noexcept = default;

    // Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
    // not be called implicitly.". std::visit Throws std::bad_variant_access if
    // as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
    // valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
    // that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
    // an exception.
    // coverity[autosar_cpp14_a15_5_3_violation]
    pointer get() const noexcept
    {
        auto ptr_visitor = score::cpp::overload(
            // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
            // expression statement and identifier declaration shall be placed on a
            // separate line.". Following line statement is fine, this happens due to
            // clang formatting.
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const lola::SamplePtr<SampleType>& lola_ptr) noexcept -> pointer {
                return lola_ptr.get();
            },
            // Suppress "AUTOSAR C++14 A8-4-12" rule finding. This rule states: "A std::unique_ptr shall be passed to a
            // function as: (1) a copy to express the function assumes ownership (2) an lvalue reference to express that
            // the function replaces the managed object"
            // This is a false positive, we here using lvalue reference.
            // coverity[autosar_cpp14_a8_4_12_violation : FALSE]
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const mock_binding::SamplePtr<SampleType>& mock_ptr) noexcept -> pointer {
                return mock_ptr.get();
            },
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const score::cpp::blank&) noexcept -> pointer {
                return nullptr;
            });
        return std::visit(ptr_visitor, binding_sample_ptr_);
    }

    // coverity[autosar_cpp14_a15_5_3_violation]
    pointer Get() const noexcept
    {
        return get();
    }

    /// \brief deref underlying managed object.
    ///
    /// Only enabled if the SampleType is not void
    /// \return ref of managed object.
    template <class T = SampleType, typename std::enable_if<!std::is_same<T, void>::value>::type* = nullptr>
    // coverity[autosar_cpp14_a15_5_3_violation]
    std::add_lvalue_reference_t<element_type> operator*() const noexcept
    {
        return *get();
    }

    // coverity[autosar_cpp14_a15_5_3_violation]
    pointer operator->() const noexcept
    {
        return get();
    }

    explicit operator bool() const noexcept
    {
        return std::holds_alternative<score::cpp::blank>(binding_sample_ptr_) == false;
    }

    void Swap(SamplePtr& other) noexcept
    {
        // Search for custom swap functions via ADL, and use std::swap if none are found.
        using std::swap;

        swap(binding_sample_ptr_, other.binding_sample_ptr_);
        swap(reference_guard_, other.reference_guard_);
    }

    void Reset(SamplePtr other = nullptr) noexcept
    {
        Swap(other);
    }

  private:
    std::variant<score::cpp::blank, lola::SamplePtr<SampleType>, mock_binding::SamplePtr<SampleType>>
        binding_sample_ptr_;
    SampleReferenceGuard reference_guard_;
};

template <typename SampleType>
// Suppress "AUTOSAR C++14 A13-5-5", The rule states: "Comparison operators shall be non-member functions with identical
// parameter types and noexcept.". There is no functional reason behind, and SamplePtr has an internal semantic to be
// compared to nullptr.
// coverity[autosar_cpp14_a13_5_5_violation]
bool operator==(std::nullptr_t /* ptr */, const SamplePtr<SampleType>& ptr) noexcept
{
    return !static_cast<bool>(ptr);
}

template <typename SampleType>
// coverity[autosar_cpp14_a13_5_5_violation]
bool operator==(const SamplePtr<SampleType>& ptr, std::nullptr_t /* ptr */) noexcept
{
    return nullptr == ptr;
}

template <typename SampleType>
// coverity[autosar_cpp14_a13_5_5_violation]
bool operator!=(std::nullptr_t /* ptr */, const SamplePtr<SampleType>& ptr) noexcept
{
    return !(nullptr == ptr);
}

template <typename SampleType>
// coverity[autosar_cpp14_a13_5_5_violation]
bool operator!=(const SamplePtr<SampleType>& ptr, std::nullptr_t /* ptr */) noexcept
{
    return nullptr != ptr;
}

template <typename SampleType>
void swap(SamplePtr<SampleType>& lhs, SamplePtr<SampleType>& rhs) noexcept
{
    lhs.Swap(rhs);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SAMPLE_PTR_H
