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
#ifndef SCORE_MW_COM_IMPL_SAMPLE_ALLOCATEE_GUARD_H
#define SCORE_MW_COM_IMPL_SAMPLE_ALLOCATEE_GUARD_H

namespace score::mw::com::impl
{

class SampleAllocateeTracker;

/// \brief RAII guard for a sample allocation.
///
/// \details When this guard is destroyed, it decrements the allocation count in the associated SampleAllocateeTracker.
/// This class is move-only to ensure each allocation is tracked exactly once.
class SampleAllocateeGuard final
{
  public:
    /// \brief Default constructs a null guard that does not track any allocation.
    ///
    /// \details In production code, guards must be obtained via SampleAllocateeTracker::Allocate() so that
    /// every allocation is tracked. Direct default construction produces a no-op guard and is therefore
    /// only intended for use in tests, where a real SampleAllocateeTracker is not available.
    SampleAllocateeGuard() = default;

    SampleAllocateeGuard(SampleAllocateeGuard&&) noexcept;
    SampleAllocateeGuard& operator=(SampleAllocateeGuard&&) noexcept;
    SampleAllocateeGuard(const SampleAllocateeGuard&) = delete;
    SampleAllocateeGuard& operator=(const SampleAllocateeGuard&) = delete;

    /// \brief Decrements the outstanding-allocation count in the associated tracker, if this guard owns a tracked
    /// allocation.
    ~SampleAllocateeGuard() noexcept;

  private:
    // Only SampleAllocateeTracker can construct non-null guards
    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision: this is the only class that shall be able to construct non-null SampleAllocateeGuard
    // instances via the private explicit constructor. This enforces the invariant that every guard is created
    // through a tracked Allocate() call, preventing untracked allocations.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SampleAllocateeTracker;

    explicit SampleAllocateeGuard(SampleAllocateeTracker& tracker);

    /// \brief Decrements the tracker count and clears tracker_, shared by the destructor and move-assignment to
    /// avoid duplicating the null-check logic.
    void Deallocate();

    /// \brief Pointer to the associated tracker, serving two purposes:
    /// - Provides access to SampleAllocateeTracker so that Deallocate() can be called on destruction.
    /// - Acts as an "active" flag, a nullptr value indicates that this guard has been moved-from or
    ///   default-constructed and does not own an allocation, while a non-null value indicates ownership.
    SampleAllocateeTracker* tracker_{nullptr};
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SAMPLE_ALLOCATEE_GUARD_H
