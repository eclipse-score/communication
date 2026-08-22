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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SAMPLE_ALLOCATEE_PTR_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SAMPLE_ALLOCATEE_PTR_H

#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"

#include <functional>
#include <limits>
#include <memory>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief SampleAllocateePtr behaves as unique_ptr to an allocated sample (event slot). It is type-erased as generally
/// our binding layer is type-erased (i.e. it just moves bytes around).
/// A user might manipulate the
/// value of the underlying pointer in any regard. If the value shall be transmitted to any consumer Send() most be
/// invoked.
/// If the pointer gets destroyed without invoking Send(), the changed data will be lost.
///
/// This type should not be created on its own, rather it shall be created by an Allocate() call towards an event. In
/// any case this type is the binding specific representation of an SampleAllocateePtr.
class SampleAllocateePtr
{
    // friends to the View wrapper; is used to access managed obj and event_data_control_ptr_
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

    /// \brief ctor creates valid SampleAllocateePtr from its members.
    /// \param ptr pointer to managed object
    /// \param event_data_ctrl event data control structure, which manages the underlying event/sample in shmem.
    /// \param slot_index index of event slot
    SampleAllocateePtr(pointer ptr,
                       EventDataControlComposite<>& event_data_ctrl,
                       ConsumerEventDataControlLocalView<>& consumer_event_data_control_local_view,
                       const SlotIndexType slot_index) noexcept;

    /// \brief SampleAllocateePtr is not copyable.
    SampleAllocateePtr(const SampleAllocateePtr&) = delete;

    /// \brief SampleAllocateePtr is movable.
    SampleAllocateePtr(SampleAllocateePtr&& other) noexcept;

    /// \brief dtor discards underlying event in event slot (if we have a valid event slot)
    ~SampleAllocateePtr() noexcept;

    /// \brief returns managed object.
    pointer get() const noexcept
    {
        return managed_object_;
    }

    /// \brief reset managed object and eventually discard underlying event slot.
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
    /// \return reference to nullptr assigned (invalid) SampleAllocateePtr
    SampleAllocateePtr& operator=(std::nullptr_t /* ptr */) & noexcept;

    /// \brief SampleAllocateePtr is not copy assignable.
    SampleAllocateePtr& operator=(const SampleAllocateePtr& other) & = delete;

    /// \brief SampleAllocateePtr is move assignable.
    SampleAllocateePtr& operator=(SampleAllocateePtr&& other) & noexcept;

    /// \brief access to internal slot index
    /// \return slot index of underlying shmem event slot.
    SlotIndexType GetReferencedSlot() const noexcept
    {
        return event_slot_index_;
    }

  private:
    static constexpr SlotIndexType kUninitialisedEventSlotIndex = std::numeric_limits<SlotIndexType>::max();

    void internal_delete();

    pointer managed_object_;
    SlotIndexType event_slot_index_;
    /// \brief Pointer to EventDataControlComposite owned by the SkeletonEvent.
    /// \details This is a non-owning pointer. The SkeletonEvent owns the EventDataControlComposite and must outlive
    /// any SampleAllocateePtr instances created from it. The SampleAllocateeTracker ensures this by terminating
    /// if SampleAllocateePtr instances outlive the SkeletonEvent. Can only be a nullptr if SampleAllocateePtr is
    /// default constructed. This should be a pointer since the SampleAllocateePtr needs to update the ignore_qm_control
    /// flag in certain situations
    EventDataControlComposite<>* event_data_control_ptr_;
    ConsumerEventDataControlLocalView<>* consumer_event_data_control_local_view_;
};

/// \brief Specializes the std::swap algorithm for SampleAllocateePtr. Swaps the contents of lhs and rhs. Calls
/// lhs.swap(rhs)
void swap(SampleAllocateePtr& lhs, SampleAllocateePtr& rhs) noexcept;

/// \brief SampleAllocateePtr is user facing, in order to interact with its internals we provide a view towards it
class SampleAllocateePtrView
{
  public:
    explicit SampleAllocateePtrView(const SampleAllocateePtr& ptr) : ptr_{ptr} {}

    const EventDataControlComposite<>& GetEventDataControlComposite() const noexcept;

    typename SampleAllocateePtr::pointer GetManagedObject() const noexcept;

  private:
    const SampleAllocateePtr& ptr_;
};

/// \brief SampleAllocateePtr is user facing, in order to interact with its internals we provide a view towards it
class SampleAllocateePtrMutableView
{
  public:
    explicit SampleAllocateePtrMutableView(SampleAllocateePtr& ptr) : ptr_{ptr} {}

    EventDataControlComposite<>& GetEventDataControlComposite() const noexcept;

    [[nodiscard]]
    ConsumerEventDataControlLocalView<>& GetConsumerEventDataControlLocalView();

  private:
    SampleAllocateePtr& ptr_;
};

// SampleAllocateePtr stores a raw pointer to EventDataControlComposite. If the composite were
// moved or copied while a SampleAllocateePtr is alive, that pointer would become invalid (UB).
// These assertions enforce at compile time that EventDataControlComposite remains non-copyable
// and non-moveable.
static_assert(!std::is_copy_constructible_v<EventDataControlComposite<>>,
              "EventDataControlComposite must not be copy constructible - SampleAllocateePtr holds a "
              "raw pointer to it and a copy would silently invalidate that pointer");
static_assert(!std::is_move_constructible_v<EventDataControlComposite<>>,
              "EventDataControlComposite must not be move constructible - SampleAllocateePtr holds a "
              "raw pointer to it and a move would silently invalidate that pointer");
static_assert(!std::is_copy_assignable_v<EventDataControlComposite<>>,
              "EventDataControlComposite must not be copy assignable - SampleAllocateePtr holds a "
              "raw pointer to it and a copy would silently invalidate that pointer");
static_assert(!std::is_move_assignable_v<EventDataControlComposite<>>,
              "EventDataControlComposite must not be move assignable - SampleAllocateePtr holds a "
              "raw pointer to it and a move would silently invalidate that pointer");

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SAMPLE_ALLOCATEE_PTR_H
