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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SAMPLE_ALLOCATEE_PTR_H
#define SCORE_MW_COM_IMPL_PLUMBING_SAMPLE_ALLOCATEE_PTR_H

#include "score/mw/com/impl/bindings/lola/sample_allocatee_ptr.h"

#include <score/blank.hpp>
#include <score/overload.hpp>

#include <cstddef>
#include <memory>
#include <utility>
#include <variant>

namespace score::mw::com::impl
{

/// \brief Pointer to a data sample allocated by the Communication Management implementation (mimics std::unique_ptr)
///
/// \details We try to implement certain functionality that facades an std::unique_ptr, but some functionalities (e.g.
/// custom deleter) are not implemented since they would provoke error prone usage. Sticking with the case of a custom
/// deleter, since the memory is allocated by the middleware, we also need to ensure that we reclaim the memory. If this
/// is overwritten (or intercepted by the user), we will provoke a memory leak.
///
/// \pre Created by Allocate() call towards a specific event.
template <typename SampleType>
class SampleAllocateePtr
{
  public:
    /// \brief SampleType*, raw-pointer to the object managed by this SampleAllocateePtr
    using pointer = SampleType*;

    /// \brief The type of the object managed by this SampleAllocateePtr
    using element_type = SampleType;

    /// \brief Constructs a SampleAllocateePtr that owns nothing.
    // Suppress "AUTOSAR C++14 A8-5-0", The rule states: "All memory shall be initialized before it is read.".
    // This is a false positive, constructor delegation guarantees member initialization.
    // coverity[autosar_cpp14_a8_5_0_violation : FALSE]
    constexpr SampleAllocateePtr() noexcept : SampleAllocateePtr{score::cpp::blank{}} {}

    /// \brief Constructs a SampleAllocateePtr that owns nothing.
    // Suppress "AUTOSAR C++14 A8-5-0", The rule states: "All memory shall be initialized before it is read.".
    // Constructor delegation guarantees member initialization, this warning is a false positive.
    // coverity[autosar_cpp14_a8_5_0_violation]
    constexpr explicit SampleAllocateePtr(std::nullptr_t /* ptr */) noexcept : SampleAllocateePtr() {}

    // no copy due to unique ownership
    SampleAllocateePtr(const SampleAllocateePtr<SampleType>&) = delete;
    SampleAllocateePtr& operator=(const SampleAllocateePtr<SampleType>&) & = delete;

    /// \brief Constructs a SampleAllocateePtr by transferring ownership from other to *this.
    SampleAllocateePtr(SampleAllocateePtr<SampleType>&& other) noexcept : SampleAllocateePtr()
    {
        this->Swap(other);
    }

    /// \brief Move assignment operator. Transfers ownership from other to *this
    SampleAllocateePtr& operator=(SampleAllocateePtr<SampleType>&& other) & noexcept
    {
        this->Swap(other);
        return *this;
    }

    SampleAllocateePtr& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    ~SampleAllocateePtr() noexcept = default;

    /// \brief Replaces the managed object.
    void reset() noexcept
    {
        auto visitor = score::cpp::overload(
            // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
            // expression statement and identifier declaration shall be placed on a
            // separate line.". Following line statement is fine, this happens due to
            // clang formatting.
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](lola::SampleAllocateePtr<SampleType>& internal_ptr) noexcept -> void {
                internal_ptr.reset();
            },
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](std::unique_ptr<SampleType>& internal_ptr) noexcept -> void {
                internal_ptr.reset(nullptr);
            },
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const score::cpp::blank&) noexcept -> void {});
        std::visit(visitor, internal_);
    }

    /// \brief Swaps the managed objects of *this and another SampleAllocateePtr object other.
    void Swap(SampleAllocateePtr<SampleType>& other) noexcept
    {
        // Search for custom swap functions via ADL, and use std::swap if none are found.
        using std::swap;
        swap(internal_, other.internal_);
    }

    /// \brief Returns a pointer to the managed object or nullptr if no object is owned.
    // Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
    // not be called implicitly.". std::visit Throws std::bad_variant_access if
    // as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
    // valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
    // that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
    // an exception.
    // This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    pointer Get() const noexcept
    {
        using ReturnType = pointer;

        auto visitor = score::cpp::overload(
            // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
            // expression statement and identifier declaration shall be placed on a
            // separate line.". Following line statement is fine, this happens due to
            // clang formatting.
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const lola::SampleAllocateePtr<SampleType>& internal_ptr) noexcept -> ReturnType {
                return internal_ptr.get();
            },
            // Suppress "AUTOSAR C++14 A8-4-12" rule finding. This rule states: "A std::unique_ptr shall be passed to a
            // function as: (1) a copy to express the function assumes ownership (2) an lvalue reference to express that
            // the function replaces the managed object"
            // This is a false positive, we here using lvalue reference.
            // coverity[autosar_cpp14_a8_4_12_violation : FALSE]
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const std::unique_ptr<SampleType>& internal_ptr) noexcept -> ReturnType {
                return internal_ptr.get();
            },
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const score::cpp::blank&) noexcept -> ReturnType {
                return nullptr;
            });

        return std::visit(visitor, internal_);
    }

    /// \brief Checks whether *this owns an object, i.e. whether Get() != nullptr
    /// \return true if *this owns an object, false otherwise.
    // Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
    // not be called implicitly.". std::visit Throws std::bad_variant_access if
    // as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
    // valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
    // that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
    // an exception.
    // This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    explicit operator bool() const noexcept
    {
        auto visitor = score::cpp::overload(
            [](const lola::SampleAllocateePtr<SampleType>& internal_ptr) noexcept -> bool {
                return static_cast<bool>(internal_ptr);
            },
            // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
            // expression statement and identifier declaration shall be placed on a
            // separate line.". Following line statement is fine, this happens due to
            // clang formatting.
            // Suppress "AUTOSAR C++14 A8-4-12" rule finding. This rule states: "A std::unique_ptr shall be passed to a
            // function as: (1) a copy to express the function assumes ownership (2) an lvalue reference to express that
            // the function replaces the managed object"
            // This is a false positive, we here using lvalue reference.
            // coverity[autosar_cpp14_a8_4_12_violation : FALSE]
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const std::unique_ptr<SampleType>& internal_ptr) noexcept -> bool {
                return static_cast<bool>(internal_ptr);
            },
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const score::cpp::blank&) noexcept -> bool {
                return false;
            });

        return std::visit(visitor, internal_);
    }

    /// \brief operator* and operator-> provide access to the object owned by *this. If no object is hold, will
    /// terminate.
    // Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
    // not be called implicitly.". std::visit Throws std::bad_variant_access if
    // as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
    // valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
    // that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
    // an exception.
    // This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    typename std::add_lvalue_reference<SampleType>::type operator*() const noexcept(noexcept(*std::declval<pointer>()))
    {
        using ReturnType = typename std::add_lvalue_reference<SampleType>::type;

        auto visitor = score::cpp::overload(
            // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
            // expression statement and identifier declaration shall be placed on a
            // separate line.". Following line statement is fine, this happens due to
            // clang formatting.
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const lola::SampleAllocateePtr<SampleType>& internal_ptr) noexcept -> ReturnType {
                return *internal_ptr;
            },
            // Suppress "AUTOSAR C++14 A8-4-12" rule finding. This rule states: "A std::unique_ptr shall be passed to a
            // function as: (1) a copy to express the function assumes ownership (2) an lvalue reference to express that
            // the function replaces the managed object"
            // This is a false positive, we here using lvalue reference.
            // coverity[autosar_cpp14_a8_4_12_violation : FALSE]
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const std::unique_ptr<SampleType>& internal_ptr) noexcept -> ReturnType {
                return *internal_ptr;
            },
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const score::cpp::blank&) noexcept -> ReturnType {
                std::terminate();
            });

        return std::visit(visitor, internal_);
    }

    /// \brief operator* and operator-> provide access to the object owned by *this. If no object is hold, will
    /// terminate.
    // Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
    // not be called implicitly.". std::visit Throws std::bad_variant_access if
    // as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
    // valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
    // that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
    // an exception.
    // This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    pointer operator->() const noexcept
    {
        using ReturnType = pointer;

        auto visitor = score::cpp::overload(
            // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
            // expression statement and identifier declaration shall be placed on a
            // separate line.". Following line statement is fine, this happens due to
            // clang formatting.
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const lola::SampleAllocateePtr<SampleType>& internal_ptr) noexcept -> ReturnType {
                return internal_ptr.get();
            },
            // Suppress "AUTOSAR C++14 A8-4-12" rule finding. This rule states: "A std::unique_ptr shall be passed to a
            // function as: (1) a copy to express the function assumes ownership (2) an lvalue reference to express that
            // the function replaces the managed object"
            // This is a false positive, we here using lvalue reference.
            // coverity[autosar_cpp14_a8_4_12_violation : FALSE]
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const std::unique_ptr<SampleType>& internal_ptr) noexcept -> ReturnType {
                return internal_ptr.get();
            },
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const score::cpp::blank&) noexcept -> ReturnType {
                std::terminate();
            });

        return std::visit(visitor, internal_);
    }

  private:
    template <typename T>
    // Suppress "AUTOSAR C++14 A0-1-3" rule finding. This rule states: "Every function defined in an anonymous
    // namespace, or static function with internal linkage, or private member function shall be used.".
    // False-positive, method is used as a delegation constructor.
    // coverity[autosar_cpp14_a0_1_3_violation : FALSE]
    explicit SampleAllocateePtr(T ptr) : internal_{std::move(ptr)}
    {
    }

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Friend class required to access private constructor as we do not have everything we need public.
    // This is because we want to shield the end user from implementation details and avoid wrong usage.
    // Thus the design decision was made to introduce friend classes to access expose private parts to
    // wrappers that are then only used by the implementation.
    template <typename T>
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend auto MakeSampleAllocateePtr(T ptr) noexcept -> SampleAllocateePtr<typename T::element_type>;

    template <typename T>
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SampleAllocateePtrView;

    template <typename T>
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SampleAllocateePtrMutableView;

    // We don't use the pimpl idiom because it would require dynamic memory allocation (that we want to avoid)
    std::variant<score::cpp::blank, lola::SampleAllocateePtr<SampleType>, std::unique_ptr<SampleType>> internal_;
};

/// \brief Compares the pointer values of two SampleAllocateePtr, or a SampleAllocateePtr and nullptr
template <class T1, class T2>
bool operator==(const SampleAllocateePtr<T1>& lhs, const SampleAllocateePtr<T2>& rhs) noexcept
{
    return lhs.Get() == rhs.Get();
}

/// \brief Compares the pointer values of two SampleAllocateePtr, or a SampleAllocateePtr and nullptr
template <class T1, class T2>
bool operator!=(const SampleAllocateePtr<T1>& lhs, const SampleAllocateePtr<T2>& rhs) noexcept
{
    return lhs.Get() != rhs.Get();
}

/// \brief Specializes the std::swap algorithm for SampleAllocateePtr. Swaps the contents of lhs and rhs. Calls
/// lhs.swap(rhs)
template <class T>
void swap(SampleAllocateePtr<T>& lhs, SampleAllocateePtr<T>& rhs) noexcept
{
    lhs.swap(rhs);
}

/// \brief Helper function to create a SampleAllocateePtr within the middleware (not to be used by the user)
template <typename T>
auto MakeSampleAllocateePtr(T ptr) noexcept -> SampleAllocateePtr<typename T::element_type>
{
    return SampleAllocateePtr<typename T::element_type>{std::move(ptr)};
}

/// \brief SampleAllocateePtr is user facing, in order to interact with its internals we provide a view towards it
template <typename SampleType>
class SampleAllocateePtrView
{
  public:
    explicit SampleAllocateePtrView(const SampleAllocateePtr<SampleType>& ptr) : ptr_{ptr} {}

    /// \brief Interpret the binding independent SampleAllocateePtr as a binding specific one
    /// \return pointer to the underlying binding specific type, nullptr if this type is not the expected one
    template <typename T>
    const T* As() const noexcept
    {
        return std::get_if<T>(&ptr_.internal_);
    }

    const std::variant<score::cpp::blank, lola::SampleAllocateePtr<SampleType>, std::unique_ptr<SampleType>>&
    GetUnderlyingVariant() const noexcept
    {
        return ptr_.internal_;
    }

  private:
    const impl::SampleAllocateePtr<SampleType>& ptr_;
};

/// \brief SampleAllocateePtr is user facing, in order to interact with its internals we provide a view towards it
template <typename SampleType>
class SampleAllocateePtrMutableView
{
  public:
    explicit SampleAllocateePtrMutableView(SampleAllocateePtr<SampleType>& ptr) : ptr_{ptr} {}

    std::variant<score::cpp::blank, lola::SampleAllocateePtr<SampleType>, std::unique_ptr<SampleType>>&
    GetUnderlyingVariant() noexcept
    {
        // Suppress "AUTOSAR C++14 A9-3-1", The rule states: "Member functions shall not return non-const “raw” pointers
        // or references to private or protected data owned by the class.". This method returns a reference to the
        // internal state of the class, which is essential for allowing users to interact directly with the variant type
        // without incurring unnecessary copies.
        // coverity[autosar_cpp14_a9_3_1_violation]
        return ptr_.internal_;
    }

  private:
    impl::SampleAllocateePtr<SampleType>& ptr_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SAMPLE_ALLOCATEE_PTR_H
