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
#ifndef SCORE_MW_COM_COM_ERROR_DOMAIN_H
#define SCORE_MW_COM_COM_ERROR_DOMAIN_H

#include "score/mw/com/impl/com_error.h"

/// \api
/// \brief The com error domain header file includes the error related definitions which are specific for the
/// mw::com API.
/// \requirement SWS_CM_11264, SWS_CM_10432, SWS_CM_11327, SWS_CM_11329
namespace score::mw::com
{

/// \api
/// \brief Error codes for COM API operations as defined by AUTOSAR SWS Communication Management
using ComErrc = score::mw::com::impl::ComErrc;

/// \api
/// \brief Error domain providing human-readable messages for COM error codes
using ComErrorDomain = score::mw::com::impl::ComErrorDomain;

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_COM_ERROR_DOMAIN_H
