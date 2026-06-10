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

#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_GENERIC_SKELETON_EVENT_H_
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_GENERIC_SKELETON_EVENT_H_

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_common.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"
#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/generic_skeleton_event_binding.h"

#include <optional>

namespace score::mw::com::impl::lola
{

class TransactionLogRegistrationGuard;
class Skeleton;

/// @brief The LoLa binding implementation for a generic skeleton event.
class GenericSkeletonEvent : public GenericSkeletonEventBinding
{
  public:
    GenericSkeletonEvent(Skeleton& parent,
                         const std::string_view event_name,
                         const SkeletonEventProperties& event_properties,
                         const ElementFqId& element_fq_id,
                         const DataTypeMetaInfo& size_info,
                         impl::tracing::SkeletonEventTracingData tracing_data = {});

    Result<void> Send(score::mw::com::impl::SampleAllocateePtr<void> sample) noexcept override;

    Result<score::mw::com::impl::SampleAllocateePtr<void>> Allocate() noexcept override;

    Result<void> Notify() noexcept override;

    std::pair<size_t, size_t> GetSizeInfo() const noexcept override;

    Result<void> PrepareOffer() noexcept override;
    void PrepareStopOffer() noexcept override;
    BindingType GetBindingType() const noexcept override;
    void SetSkeletonEventTracingData(impl::tracing::SkeletonEventTracingData tracing_data) noexcept override
    {
        skeleton_event_common_.SetSkeletonEventTracingData(tracing_data);
    }

    void SetGetterEnabled(bool getter_enabled) noexcept override
    {
        skeleton_event_common_.SetGetterEnabled(getter_enabled);
    }

    std::size_t GetMaxSize() const noexcept override
    {
        return size_info_.size;
    }

    /// \brief Set callback, to get notified, when either the 1st event-notification has been registered or the last
    /// event-notification has been unregistered.
    /// \detail This extension has been added to GenericSkeletonEvent only (not "typed" SkeletonEvent),
    /// because we are only using it so far in the gateway use case, where the gateway uses only
    /// GenericProxy/GenericSkeleton and not typed proxies/skeletons.
    Result<void> SetReceiveHandlerRegistrationChangedHandler(
        ReceiveHandlerRegistrationChangedCallback callback) noexcept override
    {
        return skeleton_event_common_.SetReceiveHandlerRegistrationChangedHandler(std::move(callback));
    }

    /// \brief Unset the callback for ReceiveHandler registration change notifications.
    Result<void> UnsetReceiveHandlerRegistrationChangedHandler() noexcept override
    {
        return skeleton_event_common_.UnsetReceiveHandlerRegistrationChangedHandler();
    }

  private:
    DataTypeMetaInfo size_info_;
    std::uint8_t* event_data_storage_;
    SkeletonEventCommon<void> skeleton_event_common_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_GENERIC_SKELETON_EVENT_H_
