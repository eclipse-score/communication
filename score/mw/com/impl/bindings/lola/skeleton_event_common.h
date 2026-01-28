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
#pragma once

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h" 
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/type_erased_sample_ptrs_guard.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include <score/assert.hpp>
#include <score/optional.hpp>
#include <score/utility.hpp>

#include <atomic>
#include <optional>
#include <string_view>

namespace score::mw::com::impl::lola
{

class Skeleton; // Forward declaration

/// @brief Common implementation for LoLa skeleton events, shared between SkeletonEvent and GenericSkeletonEvent.
class SkeletonEventCommon
{
  public:
    SkeletonEventCommon(Skeleton& parent,
                        const ElementFqId& event_fqn,
                        score::cpp::optional<EventDataControlComposite>& event_data_control_composite_ref,
                        EventSlotStatus::EventTimeStamp& current_timestamp_ref,
                        impl::tracing::SkeletonEventTracingData tracing_data = {}) noexcept;

    // No copy, no move
    SkeletonEventCommon(const SkeletonEventCommon&) = delete;
    SkeletonEventCommon(SkeletonEventCommon&&) noexcept = delete;
    SkeletonEventCommon& operator=(const SkeletonEventCommon&) & = delete;
    SkeletonEventCommon& operator=(SkeletonEventCommon&&) & noexcept = delete;

    ~SkeletonEventCommon() = default;

    ResultBlank PrepareOfferCommon() noexcept;
    void PrepareStopOfferCommon() noexcept;

    // Accessors for members used by PrepareOfferCommon/PrepareStopOfferCommon
    impl::tracing::SkeletonEventTracingData& GetTracingData() { return tracing_data_; }
    const ElementFqId& GetElementFQId() const { return event_fqn_; }
    Skeleton& GetParent() { return parent_; }

    void EmplaceTransactionLogRegistrationGuard();
    void EmplaceTypeErasedSamplePtrsGuard();
    void UpdateCurrentTimestamp();
    void SetQmNotificationsRegistered(bool value);
    void SetAsilBNotificationsRegistered(bool value);
    void ResetGuards() noexcept;
    
    // Accessors for atomic flags for derived classes' Send() method
    bool IsQmRegistered() const noexcept;
    bool IsAsilBRegistered() const noexcept;

  private:
    Skeleton& parent_;
    const ElementFqId event_fqn_;
    score::cpp::optional<EventDataControlComposite>& event_data_control_composite_ref_; // Reference to the optional in derived class
    EventSlotStatus::EventTimeStamp& current_timestamp_ref_; // Reference to the timestamp in derived class
    impl::tracing::SkeletonEventTracingData tracing_data_;

    std::atomic<bool> qm_event_update_notifications_registered_{false};
    std::atomic<bool> asil_b_event_update_notifications_registered_{false};
    std::optional<lola::TransactionLogRegistrationGuard> transaction_log_registration_guard_{};
    std::optional<tracing::TypeErasedSamplePtrsGuard> type_erased_sample_ptrs_guard_{};
};

} // namespace score::mw::com::impl::lola
