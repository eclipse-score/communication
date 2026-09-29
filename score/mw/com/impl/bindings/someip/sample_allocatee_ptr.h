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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SAMPLE_ALLOCATEE_PTR_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SAMPLE_ALLOCATEE_PTR_H

#include "score/mw/com/impl/bindings/someip/event_data_storage.h"
#include "score/mw/com/impl/bindings/someip/slot_allocation_control.h"

#include <cstddef>
#include <limits>

namespace score::mw::com::impl::someip
{

/// \brief SampleAllocateePtr behaves as unique_ptr to an allocated sample (event slot). It is type-erased as generally
/// our binding layer is type-erased (i.e. it just moves bytes around).
///
/// \details Mirrors lola::SampleAllocateePtr. The difference is what has to be released when the pointer is destroyed
///          without a preceding Send(): LoLa has to discard the slot in the shared-memory EventDataControl so that
///          consumers of other processes can reuse it, whereas the SOME/IP binding only has to return the slot to its
///          process-local SlotAllocationControl of the owning SkeletonEvent. This mirrors LoLa, where
///          SampleAllocateePtr refers to the EventDataControlComposite owned by lola::SkeletonEvent rather than to
///          the SkeletonEvent itself.
class SampleAllocateePtr
{
    // Friends to the View wrappers; used to access the managed object and the owning event.
    // coverity[autosar_cpp14_a11_3_1_violation] see above
    friend class SampleAllocateePtrView;
    // coverity[autosar_cpp14_a11_3_1_violation] see above
    friend class SampleAllocateePtrMutableView;

  public:
    using pointer = void*;
    using const_pointer = void* const;
    using element_type = void;

    /// \brief default ctor giving invalid SampleAllocateePtr (owning no managed object, invalid event slot)
    explicit SampleAllocateePtr() noexcept : SampleAllocateePtr{nullptr} {}

    /// \brief ctor from nullptr_t also giving invalid SampleAllocateePtr like default ctor.
    explicit SampleAllocateePtr(std::nullptr_t /* ptr */) noexcept;

    /// \brief ctor creating a valid SampleAllocateePtr from its members.
    /// \param ptr pointer to the managed (type-erased) slot
    /// \param slot_allocation_control control which handed out the slot and which takes it back on destruction
    /// \param slot_index index of the slot within the event's EventDataStorage
    SampleAllocateePtr(pointer ptr,
                       SlotAllocationControl& slot_allocation_control,
                       const SlotIndexType slot_index) noexcept;

    /// \brief SampleAllocateePtr is not copyable.
    SampleAllocateePtr(const SampleAllocateePtr&) = delete;

    /// \brief SampleAllocateePtr is movable.
    SampleAllocateePtr(SampleAllocateePtr&& other) noexcept;

    /// \brief dtor discards the underlying slot (if we have a valid slot)
    ~SampleAllocateePtr() noexcept;

    /// \brief returns the managed object.
    pointer get() const noexcept
    {
        return managed_object_;
    }

    /// \brief reset managed object and eventually discard the underlying slot.
    void reset() noexcept;

    /// \brief swap content with _other_
    void swap(SampleAllocateePtr& other) noexcept;

    /// \brief check validity.
    /// \return true, if SampleAllocateePtr owns a valid managed object
    explicit operator bool() const noexcept
    {
        return managed_object_ != nullptr;
    }

    /// \brief assign nullptr.
    SampleAllocateePtr& operator=(std::nullptr_t /* ptr */) & noexcept;

    /// \brief SampleAllocateePtr is not copy assignable.
    SampleAllocateePtr& operator=(const SampleAllocateePtr& other) & = delete;

    /// \brief SampleAllocateePtr is move assignable.
    SampleAllocateePtr& operator=(SampleAllocateePtr&& other) & noexcept;

    /// \brief access to internal slot index
    SlotIndexType GetReferencedSlot() const noexcept
    {
        return event_slot_index_;
    }

  private:
    static constexpr SlotIndexType kUninitialisedEventSlotIndex = std::numeric_limits<SlotIndexType>::max();

    void internal_delete() noexcept;

    pointer managed_object_;
    SlotIndexType event_slot_index_;
    /// \brief Non-owning pointer to the SlotAllocationControl owned by the SkeletonEvent which handed out the slot.
    /// The SkeletonEvent (and with it the control) must outlive any SampleAllocateePtr created from it; this is
    /// guaranteed by the SampleAllocateeTracker on the binding independent layer. Can only be nullptr if the
    /// SampleAllocateePtr is default/nullptr constructed.
    SlotAllocationControl* slot_allocation_control_;
};

/// \brief Specializes the std::swap algorithm for SampleAllocateePtr. Swaps the contents of lhs and rhs.
void swap(SampleAllocateePtr& lhs, SampleAllocateePtr& rhs) noexcept;

/// \brief SampleAllocateePtr is user facing, in order to interact with its internals we provide a view towards it
class SampleAllocateePtrView
{
  public:
    explicit SampleAllocateePtrView(const SampleAllocateePtr& ptr) : ptr_{ptr} {}

    typename SampleAllocateePtr::pointer GetManagedObject() const noexcept;

  private:
    const SampleAllocateePtr& ptr_;
};

/// \brief SampleAllocateePtr is user facing, in order to interact with its internals we provide a view towards it
class SampleAllocateePtrMutableView
{
  public:
    explicit SampleAllocateePtrMutableView(SampleAllocateePtr& ptr) : ptr_{ptr} {}

    SlotAllocationControl& GetSlotAllocationControl() const noexcept;

  private:
    SampleAllocateePtr& ptr_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SAMPLE_ALLOCATEE_PTR_H
