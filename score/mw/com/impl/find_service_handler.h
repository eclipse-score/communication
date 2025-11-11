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
#ifndef SCORE_MW_COM_IMPL_FIND_SERVICE_HANDLER_H
#define SCORE_MW_COM_IMPL_FIND_SERVICE_HANDLER_H

#include "score/mw/com/impl/find_service_handle.h"

#include <score/callback.hpp>

#include <vector>

namespace score::mw::com::impl
{

/// \api
/// \brief The container holds a list of service handles and is used as a return value of the FindService methods.
/// \requirement SWS_CM_00304
/// \public
template <typename T>
using ServiceHandleContainer = std::vector<T>;

/// \api
/// \brief A function wrapper for the handler function that gets called by the Communication Management software in case
/// the service availability changes.
///
/// \details It takes as input parameter a handle container containing handles for all matching service instances and a
/// FindServiceHandle which can be used to invoke StopFindService (see [SWS_CM_00125]) from within the
/// FindServiceHandler.
///
/// \requirement SWS_CM_00383
/// \public
template <typename T>
using FindServiceHandler = score::cpp::callback<void(ServiceHandleContainer<T>, FindServiceHandle)>;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_FIND_SERVICE_HANDLER_H
