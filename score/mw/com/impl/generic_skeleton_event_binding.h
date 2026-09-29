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
#ifndef SCORE_MW_COM_IMPL_GENERIC_SKELETON_EVENT_BINDING_H
#define SCORE_MW_COM_IMPL_GENERIC_SKELETON_EVENT_BINDING_H

#include "score/mw/com/impl/receive_handler_registration_changed_handler.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include "score/result/result.h"

#include <score/callback.hpp>

namespace score::mw::com::impl
{

/// \brief Interface a generic skeleton event binding implementation has to implement.
///
/// \details GenericSkeletonEventBinding is also fully type-erased like the SkeletonEventBinding, but has some
/// additional functionality on top of it, thus it subclasses SkeletonEventBinding and adds some additional methods. All
/// generic skeleton event binding implementations are required to derive from this class. The additional functionality
/// GenericSkeletonEventBinding provides, comes mainly from gateway use-cases. I.e., we see the usage of
/// GenericSkeletonEventBinding mainly in gateway scenarios.
class GenericSkeletonEventBinding : public SkeletonEventBinding
{
  public:
    virtual ~GenericSkeletonEventBinding() = default;

    GenericSkeletonEventBinding() = default;
    // A GenericSkeletonEventBinding is always held via a pointer in the binding independent impl::SkeletonEvent.
    // Therefore, the binding itself doesn't have to be moveable or copyable, as the pointer can simply be copied when
    // moving the impl::SkeletonEvent.
    GenericSkeletonEventBinding(const GenericSkeletonEventBinding&) = delete;
    GenericSkeletonEventBinding(GenericSkeletonEventBinding&&) noexcept = delete;
    GenericSkeletonEventBinding& operator=(const GenericSkeletonEventBinding&) & = delete;
    GenericSkeletonEventBinding& operator=(GenericSkeletonEventBinding&&) & noexcept = delete;

    /// \brief Trigger notification of potential registered receive handlers.
    /// \details This is a specific API for the gateway use-case!
    virtual Result<void> Notify() noexcept = 0;

    /// \brief Sets a callback that will be called when the first ReceiveHandler of a GenericEvent
    /// will get registered or the last ReceiveHandler will be removed.
    /// \details This is a specific API for the gateway use-case!
    virtual Result<void> SetReceiveHandlerRegistrationChangedHandler(
        ReceiveHandlerRegistrationChangedCallback callback) noexcept = 0;

    virtual Result<void> UnsetReceiveHandlerRegistrationChangedHandler() noexcept = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_SKELETON_EVENT_BINDING_H
