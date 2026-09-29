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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SAMPLE_PTR_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SAMPLE_PTR_H

#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/slot_decrementer.h"

#include <optional>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl::lola
{

class TransactionLogSet;

/// \brief SamplePtr behaves as unique_ptr to a sample (event slot). User get access to a SamplePtr via GetNewSamples().
/// \details This is the LoLa binding specific SamplePtr, which holds a link to the underlying slot in shared memory.
/// It is type-erased as our binding layer is always type-erased.
class SamplePtr final
{
  public:
    using pointer = const void*;
    using element_type = void;

    /// \brief default ctor giving invalid SamplePtr (owning no managed object, invalid event slot)
    SamplePtr() noexcept : SamplePtr{nullptr, std::nullopt, EventSlotStatus::INVALID_TIMESTAMP} {}

    /// \brief ctor from nullptr_t also giving invalid SamplePtr like default ctor.
    explicit SamplePtr(std::nullptr_t /* ptr */) noexcept
        : SamplePtr{nullptr, std::nullopt, EventSlotStatus::INVALID_TIMESTAMP}
    {
    }

    /// \brief ctor creates valid SamplePtr from its members.
    /// \param ptr pointer to managed object
    /// \param event_data_ctrl_local event data control structure, which manages the underlying event/sample in shmem.
    /// \param slot_index index of event slot
    SamplePtr(pointer ptr,
              ConsumerEventDataControlLocalView<>& event_data_ctrl_local,
              const SlotIndexType slot_index) noexcept
        : SamplePtr{ptr,
                    std::make_optional<SlotDecrementer>(event_data_ctrl_local, slot_index),
                    (event_data_ctrl_local)[slot_index].GetTimeStamp()}
    {
    }

    ~SamplePtr() noexcept = default;

    /// \brief assign nullptr.
    /// \return reference to nullptr assigned (invalid) SamplePtr
    SamplePtr& operator=(std::nullptr_t) & noexcept
    {
        managed_object_ = nullptr;
        slot_decrementer_ = {};
        return *this;
    }

    /// \brief SamplePtr is not copyable.
    SamplePtr& operator=(const SamplePtr& other) noexcept = delete;
    SamplePtr(const SamplePtr&) noexcept = delete;

    /// \brief SamplePtr is moveable.
    SamplePtr& operator=(SamplePtr&& other) noexcept = default;
    SamplePtr(SamplePtr&& other) noexcept = default;

    /// \brief returns managed object.
    /// \todo: Maybe remove later, if not used anymore by user facing wrappers.
    pointer get() const noexcept
    {
        return managed_object_;
    };

    /// \brief check validity.
    /// \return true, if SamplePtr owns a valid managed object
    explicit operator bool() const noexcept
    {
        return managed_object_ != nullptr;
    }

    /// \brief Compares two SamplePtr instances based on their timestamp
    /// \param other SamplePtr to compare against
    /// \return true if this instance is older than \p other, false otherwise or if any of the SamplePtr are invalid
    bool operator<(const SamplePtr& other) const noexcept
    {
        if (!(*this) || !other)
        {
            return false;
        }

        return timestamp_ < other.timestamp_;
    }

    /// \brief Compares two SamplePtr instances based on their timestamp
    /// \param other SamplePtr to compare against
    /// \return true if this instance is newer than \p other, false otherwise or if any of the SamplePtr are invalid
    bool operator>(const SamplePtr& other) const noexcept
    {
        if (!(*this) || !other)
        {
            return false;
        }
        return timestamp_ > other.timestamp_;
    }

  private:
    explicit SamplePtr(pointer managed_object,
                       std::optional<SlotDecrementer>&& slot_decrementer,
                       EventSlotStatus::EventTimeStamp timestamp) noexcept
        : managed_object_{managed_object}, slot_decrementer_{std::move(slot_decrementer)}, timestamp_{timestamp}
    {
    }

    pointer managed_object_;
    std::optional<SlotDecrementer> slot_decrementer_;
    EventSlotStatus::EventTimeStamp timestamp_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SAMPLE_PTR_H
