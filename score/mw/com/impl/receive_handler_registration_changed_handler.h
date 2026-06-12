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
#ifndef SCORE_MW_COM_IMPL_REGISTRATION_CHANGE_HANDLER_H
#define SCORE_MW_COM_IMPL_REGISTRATION_CHANGE_HANDLER_H

#include <score/callback.hpp>

namespace score::mw::com::impl
{

/// \brief Callback that will be called if the first ReceiveHandler of a GenericEvent will get registered or the last
/// ReceiveHandler will be removed.
/// \details The callback's boolean parameter will be true if the first handler has been registered and false if the
/// last handler has been withdrawn.
using ReceiveHandlerRegistrationChangedCallback = score::cpp::callback<void(bool)>;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_REGISTRATION_CHANGE_HANDLER_H
