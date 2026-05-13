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
#ifndef SCORE_MW_COM_IMPL_SUBSCRIPTION_STATE_CHANGE_HANDLER_H
#define SCORE_MW_COM_IMPL_SUBSCRIPTION_STATE_CHANGE_HANDLER_H

#include "score/mw/com/impl/subscription_state.h"

#include <score/callback.hpp>

namespace score::mw::com::impl
{

/// \brief Callback for event/field subscription state change notifications on proxy side.
/// \details This callback may be called under lock of the internal state-machine. This in general means, that the user
/// shall not do any long-running activities within this handler as it will prolonge this lock.
/// The user also shall not call any methods on the same event/field instance for which the handler was set, as they
/// might also require the same lock, which is not guaranteed to be recursive, thus leading to a deadlock. If the user
/// intends to do such activities from this callback, he shall dispatch it to a separate thread.
/// However, unsetting the handler within its call is a reasonable/supported use-case. Instead of calling
/// UnsetSubscriptionStateChangeHandler, the user shall return false from the handler. See return-value description.
/// \param new_state new subscription state.
/// \return true if the registered handler shall be kept, false if it shall be unset/unregistered.
using SubscriptionStateChangeHandler = score::cpp::callback<bool(SubscriptionState new_state)>;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SUBSCRIPTION_STATE_CHANGE_HANDLER_H
