/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_LIB_MEMORY_SHARED_OFFSET_REF_H
#define SCORE_LIB_MEMORY_SHARED_OFFSET_REF_H

#include "score/memory/shared/memory_region_bounds.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <score/assert.hpp>

#include <cstddef>

namespace score::memory::shared
{

/// \brief Custom implementation of an offset reference. It behaves similarly to a native C++ reference (T&) but, like
/// OffsetPtr, is safe to use within shared memory that is mapped at different virtual addresses in different
/// processes.
///
/// \details Internally, OffsetRef stores an offset (relative to its own address) to the referenced object, exactly like
/// OffsetPtr does, and re-uses OffsetPtr's bounds-checking mechanism (detail_offset_ptr::AssertOffsetBoundsCheck()).
/// In contrast to OffsetPtr, OffsetRef:
/// - can only be constructed from a valid reference to T (never from a raw pointer).
/// - can never be "null"/nullptr.
/// - is neither copyable nor moveable.
///
/// Because of these two restrictions, OffsetRef does not need to reserve/handle a "null" offset representation, nor
/// does it need to store bounds information for the "copied out of shared memory" scenario that OffsetPtr has to
/// handle (that information is only needed to preserve correct bounds-checking when an OffsetPtr is copied to a
/// location outside of the shared-memory region it originally referred to; since OffsetRef can never be copied/moved,
/// this scenario cannot occur). Consequently, OffsetRef only stores the offset itself (i.e. it has the same size as a
/// single pointer/std::ptrdiff_t), unlike OffsetPtr which additionally stores a MemoryRegionBounds.
///
/// The main difference with a native C++ reference is that this is an object and occupies memory. A native C++
/// reference is just a name alias for an object that might end up occupying memory in certain scenarios due to how it
/// is implemented.
///
/// \attention It is up to the user to verify the validity of the referenced object before accessing it (e.g. make
/// sure that it is not yet destructed or moved-from), exactly as it is up to the user for a native reference.
template <typename T>
// Suppress "AUTOSAR C++14 A12-0-1", The rule states: "If a class declares a copy or move operation, or a destructor,
// either via "=default", "=delete", or via a user-provided declaration, then all others of these five special member
// functions shall be declared as well."
// Rationale: All five special member functions are declared explicitly below (copy/move are deleted, destructor is
// defaulted).
// NOLINTBEGIN(cppcoreguidelines-special-member-functions): all five are declared explicitly below.
// coverity[autosar_cpp14_a12_0_1_violation : FALSE]
class OffsetRef
// NOLINTEND(cppcoreguidelines-special-member-functions)
{
  public:
    using value_type = T;

    /// \brief Constructs an OffsetRef referencing ref.
    /// \param ref the object to reference. Must remain valid for at least as long as this OffsetRef is used.
    /// \details It is noexcept, mirroring OffsetPtr's constructor. The precondition check below (like OffsetPtr's own
    /// null-representation check in CalculateOffsetFromPointer) is therefore only verifiable via an EXPECT_DEATH test
    /// rather than SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED.
    explicit OffsetRef(T& ref) noexcept : offset_{SubtractPointersBytes(&ref, this)}
    {
        // Unlike OffsetPtr, OffsetRef has no "null" representation to guard against. Instead, since ref must be a
        // distinct object from this OffsetRef, we assert that the referenced object's memory footprint
        // ([this + offset_, this + offset_ + sizeof(T))) does not overlap with this OffsetRef's own footprint
        // ([this, this + sizeof(OffsetRef<T>))). Such an overlap would mean that constructing this OffsetRef
        // corrupts (part of) the very object it is meant to reference.
        const auto self_size = static_cast<detail_offset_ptr::difference_type>(sizeof(OffsetRef<T>));
        const auto referenced_size = static_cast<detail_offset_ptr::difference_type>(sizeof(T));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE((offset_ >= self_size) || ((offset_ + referenced_size) <= 0),
                                                    "OffsetRef must not overlap with the referenced object.");
    }

    // OffsetRef behaves like a native reference: it cannot be re-bound to reference a different object, so it is
    // neither copyable nor moveable.
    OffsetRef(const OffsetRef&) = delete;
    OffsetRef& operator=(const OffsetRef&) = delete;
    OffsetRef(OffsetRef&&) = delete;
    OffsetRef& operator=(OffsetRef&&) = delete;
    ~OffsetRef() = default;

    /// \brief Assigns value to the referenced object (does NOT re-bind this OffsetRef), mirroring the assignment
    /// semantics of a native C++ reference.
    OffsetRef& operator=(const T& value)
    {
        get() = value;
        return *this;
    }

    /// \brief Returns a reference to the referenced object (with bounds-checking).
    T& get() const
    {
        // OffsetRef never needs to capture bounds for the "copied out of shared memory" scenario (since it is never
        // copied), so we always pass a default constructed (empty) MemoryRegionBounds here.
        detail_offset_ptr::AssertOffsetBoundsCheck(
            this, offset_, MemoryRegionBounds{}, sizeof(T), sizeof(OffsetRef<T>));

        void* const referenced_address =
            // NOLINTNEXTLINE(score-banned-function): AddOffsetToPointer is the sanctioned way of doing pointer
            // arithmetic in this codebase (see justification comment at the top of offset_ptr.h).
            AddOffsetToPointer(static_cast<const void*>(this), offset_);
        return *static_cast<T*>(referenced_address);
    }

    // Suppress "AUTOSAR C++14 A13-5-2" rule finding: "All user-defined conversion operators shall be defined
    // explicit."
    // Rationale: OffsetRef is intended to be usable wherever a native T& would be usable, which requires this
    // conversion operator to be implicit.
    // NOLINTBEGIN(google-explicit-constructor): needed implicit conversion, see rationale above.
    // coverity[autosar_cpp14_a13_5_2_violation]
    operator T&() const
    {
        return get();
    }
    // NOLINTEND(google-explicit-constructor)

  private:
    /// \brief Offset which represents the number of bytes to the referenced object relative to this OffsetRef's own
    /// address.
    detail_offset_ptr::difference_type offset_;
};

template <typename T>
bool operator==(const OffsetRef<T>& lhs, const OffsetRef<T>& rhs)
{
    return lhs.get() == rhs.get();
}

template <typename T>
bool operator!=(const OffsetRef<T>& lhs, const OffsetRef<T>& rhs)
{
    return !(lhs == rhs);
}

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_OFFSET_REF_H
