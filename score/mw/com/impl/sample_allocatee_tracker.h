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
#ifndef SCORE_MW_COM_IMPL_SAMPLE_ALLOCATEE_TRACKER_H
#define SCORE_MW_COM_IMPL_SAMPLE_ALLOCATEE_TRACKER_H

#include "score/mw/com/impl/sample_allocatee_guard.h"

#include <atomic>

namespace score::mw::com::impl
{

/// \brief Tracks the number of outstanding SampleAllocateePtr instances for a SkeletonEvent.
///
/// \details This class is used to ensure that all SampleAllocateePtr instances are released before the SkeletonEvent
/// is destroyed. Similar to SampleReferenceTracker on the proxy side, but simpler since skeleton side doesn't need
/// the same pre-allocation pattern, it just needs to track allocations.
class SampleAllocateeTracker final
{
  public:
    SampleAllocateeTracker() = default;

    SampleAllocateeTracker(SampleAllocateeTracker&&) = delete;
    SampleAllocateeTracker(const SampleAllocateeTracker&) = delete;
    SampleAllocateeTracker& operator=(SampleAllocateeTracker&&) = delete;
    SampleAllocateeTracker& operator=(const SampleAllocateeTracker&) = delete;

    /// \brief Aborts if any SampleAllocateePtr instances are still outstanding at destruction time.
    ~SampleAllocateeTracker() noexcept;

    /// \brief Creates a guard that tracks one allocated sample.
    /// \return A guard that will decrement the counter when destroyed.
    SampleAllocateeGuard Allocate();

    /// \brief Returns the number of currently allocated SampleAllocateeGuards.
    std::size_t GetNumAllocated() const;

  private:
    // SampleAllocateeGuard needs access to Deallocate().
    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision: SampleAllocateeGuard needs to call the private Deallocate() method when the guard is destroyed
    // or moved-from. Keeping Deallocate() private ensures that only the RAII guard can decrement the allocation count,
    // preventing misuse by external callers.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SampleAllocateeGuard;

    /// \brief Decrements allocated_count_ by one, called exclusively by SampleAllocateeGuard on destruction or
    /// move-assignment. Keeping this private ensures the count can never be decremented by any path other than the RAII
    /// guard, so a non-zero count at tracker destruction is always a real lifecycle violation, not a misuse of the API.
    void Deallocate();

    /// \brief Number of SampleAllocateeGuard instances that are currently alive and point back to this tracker.
    std::atomic_size_t allocated_count_{0U};
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SAMPLE_ALLOCATEE_TRACKER_H
