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
/**
 * Header file for artifacts, which AUTOSAR SWS Communication Management
 * places into namespace score::mw::com::runtime.
 *
 * Right now, AUTOSAR misses to specify the corresponding header.
 * We raised a ticket @see https://jira.autosar.org/browse/AR-106018
 */
#ifndef SCORE_MW_COM_RUNTIME_H
#define SCORE_MW_COM_RUNTIME_H

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/types.h"

#include "score/memory/string_literal.h"
#include "score/result/result.h"

#include <cstdint>

namespace score::mw::com::runtime
{

/**
 * @brief Resolves given InstanceSpecifier (port name in the model) to a collection of InstanceIdentifiers via
 * manifest lookup.
 *
 * @param modelName (name of the SWC port)
 * @return container with InstanceIdentifiers
 *
 * \requirement SWS_CM_00118
 */
score::Result<score::mw::com::InstanceIdentifierContainer> ResolveInstanceIDs(const InstanceSpecifier model_name);

/// \brief Initializes mw::com subsystem with the given configuration referenced in the command-line options.
/// \detail This call is optional for a mw::com user. Only if the mw::com configuration (json) is not located in the
///         default manifest path, this function shall be called with the commandline option
///         "-service_instance_manifest" pointing to the json config file to be used.
/// \attention This function shall only be called ONCE per application/process lifetime! A second call may have no
///            effect after an internal runtime singleton has been already created/is in use!
// NOLINTNEXTLINE(modernize-avoid-c-arrays):C-style array tolerated for command line arguments
void InitializeRuntime(const std::int32_t argc, score::StringLiteral argv[]);

}  // namespace score::mw::com::runtime

#endif  // SCORE_MW_COM_RUNTIME_H
