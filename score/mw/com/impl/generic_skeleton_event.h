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
#ifndef SCORE_MW_COM_IMPL_GENERIC_SKELETON_EVENT_H_
#define SCORE_MW_COM_IMPL_GENERIC_SKELETON_EVENT_H_

#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/generic_skeleton_event_binding.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/receive_handler_registration_changed_handler.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/result/result.h"

#include <string>

namespace score::mw::com::impl
{

class GenericSkeletonEventBinding;

class GenericSkeletonEvent : public SkeletonEventBase
{
  public:
    GenericSkeletonEvent(SkeletonBase& skeleton_base,
                         const std::string_view event_name,
                         std::unique_ptr<GenericSkeletonEventBinding> binding);

    Result<void> Send(SampleAllocateePtr<void> sample) noexcept;

    Result<SampleAllocateePtr<void>> Allocate() noexcept;

    /// \brief Explicitly trigger event-update-notifications without sending new data.
    /// \note Caller must have already committed data to shared memory (gateway use).
    Result<void> Notify() noexcept;
    DataTypeMetaInfo GetSizeInfo() const noexcept;

    /// \brief Set callback, to get notified, when either the 1st event-notification has been registered or the last
    /// event-notification has been unregistered.
    /// \detail This extension has been added to GenericSkeletonEvent only (not "typed" SkeletonEvent),
    /// because we are only using it so far in the gateway use case, where the gateway uses only
    /// GenericProxy/GenericSkeleton and not typed proxies/skeletons.
    Result<void> SetReceiveHandlerRegistrationChangedHandler(
        ReceiveHandlerRegistrationChangedCallback callback) noexcept;

    /// \brief Unset the callback for receive handler registration change notifications.
    Result<void> UnsetReceiveHandlerRegistrationChangedHandler() noexcept;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_SKELETON_EVENT_H_
