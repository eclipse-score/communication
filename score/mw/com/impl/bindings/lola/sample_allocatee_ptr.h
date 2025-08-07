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

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"

#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief SampleAllocateePtr behaves as unique_ptr to an allocated sample (event slot). A user might manipulate the
/// value of the underlying pointer in any regard. If the value shall be transmitted to any consumer Send() most be
/// invoked.
/// If the pointer gets destroyed without invoking Send(), the changed data will be lost.
///
/// This type should not be created on its own, rather it shall be created by an Allocate() call towards an event. In
/// any case this type is the binding specific representation of an SampleAllocateePtr.
template <typename SampleType>
class SampleAllocateePtr
{
    // friends to the View wrapper; is used to acces managed obj and event_data_control_
    template <typename T>
    // coverity[autosar_cpp14_a11_3_1_violation] see above
    friend class SampleAllocateePtrView;
    template <typename T>
    // coverity[autosar_cpp14_a11_3_1_violation] see above
    friend class SampleAllocateePtrMutableView;

  public:
    using pointer = SampleType*;
    using const_pointer = SampleType* const;
    using element_type = SampleType;

    /// \brief default ctor giving invalid SampleAllocateePtr (owning no managed object, invalid event slot)
    // NOLINTBEGIN(cppcoreguidelines-pro-type-member-init): all members are initialized in the delegated constructor
    // Suppress "AUTOSAR C++14 M3-2-2" rule finding. This rule declares: "The One Definition Rule shall not be
    // violated.".
    // Templates are allowed to have multiple definitions across translation units as long as they are identical.
    // coverity[autosar_cpp14_m3_2_2_violation]
    explicit SampleAllocateePtr() noexcept : SampleAllocateePtr{nullptr} {}
    // NOLINTEND(cppcoreguidelines-pro-type-member-init): see above

    /// \brief ctor from nullptr_t also giving invalid SampleAllocateePtr like default ctor.
    // Suppress "AUTOSAR C++14 M3-2-2" rule finding. This rule declares: "The One Definition Rule shall not be
    // violated.".
    // Templates are allowed to have multiple definitions across translation units as long as they are identical.
    // Suppress "AUTOSAR C++14 A12-1-5" rule finding. This rule declares: "Common class initialization for non-constant
    // members shall be done by a delegating constructor.".
    // Here we need to differentiation the class initialization in case sampleAllocateePtr is nullptr and the other
    // constructor(case) if we provide a valid ptr(not null) so for sure EventDataControlComposite has a valid
    // reference. So we need to have two different class initializations. coverity[autosar_cpp14_m3_2_2_violation]
    // coverity[autosar_cpp14_a12_1_5_violation]
    explicit SampleAllocateePtr(std::nullptr_t /* ptr */) noexcept
        : managed_object_{nullptr}, event_slot_indicator_{}, event_data_control_{std::nullopt}
    {
    }

    /// \brief ctor creates valid SampleAllocateePtr from its members.
    /// \param ptr pointer to managed object
    /// \param event_data_ctrl event data control structure, which manages the underlying event/sample in shmem.
    /// \param slot_index index of event slot
    explicit SampleAllocateePtr(pointer ptr,
                                const EventDataControlComposite& event_data_ctrl,
                                ControlSlotCompositeIndicator slot_indicator) noexcept
        : managed_object_{ptr}, event_slot_indicator_{slot_indicator}, event_data_control_{event_data_ctrl}
    {
    }

    /// \brief SampleAllocateePtr is not copyable.
    SampleAllocateePtr(const SampleAllocateePtr&) = delete;

    /// \brief SampleAllocateePtr is movable.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): all members initialized by default constructor
    SampleAllocateePtr(SampleAllocateePtr&& other) noexcept : SampleAllocateePtr()
    {
        this->swap(other);
    };

    /// \brief dtor discards underlying event in event slot (if we have a valid event slot)
    ~SampleAllocateePtr() noexcept
    {
        internal_delete();
    };

    /// \brief returns managed object.
    /// \todo: Maybe remove later, if not used anymore by user facing wrappers.
    pointer get() const noexcept
    {
        return managed_object_;
    };

    /// \brief reset managed object and eventually discard underlying event slot.
    void reset(std::nullptr_t /* ptr */ = nullptr) noexcept
    {
        internal_delete();
    }
    [[deprecated("SCORE_DEPRECATION: reset shall not be used (will be also removed from user facing interface).")]] void
    reset(const_pointer p)
    {
        managed_object_ = p;
    }

    /// \brief swap content with _other_
    void swap(SampleAllocateePtr& other) noexcept
    {
        // Search for custom swap functions via ADL, and use std::swap if none are found.
        using std::swap;

        swap(this->managed_object_, other.managed_object_);
        swap(this->event_slot_indicator_, other.event_slot_indicator_);
        swap(this->event_data_control_, other.event_data_control_);
    }

    /// \brief check validity.
    /// \return true, if SampleAllocateePtr owns a valid managed object
    explicit operator bool() const noexcept
    {
        return managed_object_ != nullptr;
    }

    /// \brief deref underlying managed object.
    /// \return ref of managed object.
    typename std::add_lvalue_reference<SampleType>::type operator*() const noexcept
    {
        return *managed_object_;
    }
    /// \brief access managed object
    /// \return pointer to managed object
    pointer operator->() const noexcept
    {
        return managed_object_;
    }

    /// \brief assign nullptr.
    /// \return reference to nullptr assigned (invalid) SampleAllocateePtr
    SampleAllocateePtr& operator=(std::nullptr_t /* ptr */) & noexcept
    {
        internal_delete();
        return *this;
    }
    /// \brief SampleAllocateePtr is not copy assignable.
    SampleAllocateePtr& operator=(const SampleAllocateePtr& other) & = delete;

    /// \brief SampleAllocateePtr is move assignable.
    SampleAllocateePtr& operator=(SampleAllocateePtr&& other) & noexcept
    {
        this->swap(other);
        return *this;
    };

    /// \brief access to internal control-slot-indicator
    /// \return ControlSlotCompositeIndicator pointing to the underlying shmem event slot.
    ControlSlotCompositeIndicator GetReferencedSlot() const noexcept
    {
        return event_slot_indicator_;
    };

  private:
    void internal_delete()
    {
        managed_object_ = nullptr;
        if (event_slot_indicator_.IsValidQM() || event_slot_indicator_.IsValidAsilB())
        {
            // LCOV_EXCL_BR_START: Defensive programming: The only time that event_data_control_ does not have a value
            // is if the SampleAllocateePtr is default initialised or initialised with a nullptr. In both these cases
            // IsValidQM/IsValidAsilB will return false, so we will never enter this branch.
            if (event_data_control_.has_value())
            {
                // LCOV_EXCL_BR_STOP
                event_data_control_->Discard(event_slot_indicator_);
            }
            event_slot_indicator_.Reset();
        }
    }

    pointer managed_object_;
    ControlSlotCompositeIndicator event_slot_indicator_;
    std::optional<EventDataControlComposite> event_data_control_;
};

/// \brief Specializes the std::swap algorithm for SampleAllocateePtr. Swaps the contents of lhs and rhs. Calls
/// lhs.swap(rhs)
template <typename SampleType>
void swap(SampleAllocateePtr<SampleType>& lhs, SampleAllocateePtr<SampleType>& rhs) noexcept
{
    lhs.swap(rhs);
}

/// \brief SampleAllocateePtr is user facing, in order to interact with its internals we provide a view towards it
template <typename SampleType>
class SampleAllocateePtrView
{
  public:
    explicit SampleAllocateePtrView(const SampleAllocateePtr<SampleType>& ptr) : ptr_{ptr} {}

    const std::optional<EventDataControlComposite>& GetEventDataControlComposite() const noexcept
    {
        return ptr_.event_data_control_;
    }

    typename SampleAllocateePtr<SampleType>::pointer GetManagedObject() const noexcept
    {
        return ptr_.managed_object_;
    }

  private:
    const SampleAllocateePtr<SampleType>& ptr_;
};

/// \brief SampleAllocateePtr is user facing, in order to interact with its internals we provide a view towards it
template <typename SampleType>
class SampleAllocateePtrMutableView
{
  public:
    explicit SampleAllocateePtrMutableView(SampleAllocateePtr<SampleType>& ptr) : ptr_{ptr} {}

    const std::optional<EventDataControlComposite>& GetEventDataControlComposite() const noexcept
    {
        return ptr_.event_data_control_;
    }

  private:
    SampleAllocateePtr<SampleType>& ptr_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SAMPLE_ALLOCATEE_PTR_H
