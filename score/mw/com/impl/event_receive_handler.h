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
#ifndef SCORE_MW_COM_IMPL_EVENT_RECEIVE_HANDLER_H
#define SCORE_MW_COM_IMPL_EVENT_RECEIVE_HANDLER_H

#include <score/callback.hpp>

namespace score::mw::com::impl
{

/// \brief Callback for event notifications on proxy side.
/// \requirement SWS_CM_00309
using EventReceiveHandler = score::cpp::callback<void(void)>;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_EVENT_RECEIVE_HANDLER_H
