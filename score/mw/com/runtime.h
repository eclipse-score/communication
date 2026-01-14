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

#include "score/mw/com/mocking/i_runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/com/types.h"

#include "score/memory/string_literal.h"
#include "score/result/result.h"

#include <cstdint>

namespace score::mw::com::runtime
{
namespace detail
{

/// @brief Holder class which stores a pointer to a IRuntime which is used to mock the implementation of the
/// free-functions in this file.
///
/// The getter is public as it will be accessed by the free-functions in this file. The setter is private and can only
/// be accessed via the test-only InjectRuntimeMock function in mw/com/test_type_utilities.cpp.
class RuntimeMockHolder
{
    friend void InjectRuntimeMock(IRuntime&);

  public:
    static IRuntime* GetRuntimeMock()
    {
        return runtime_mock_;
    }

  private:
    static void InjectRuntimeMockImpl(IRuntime& runtime_mock)
    {
        runtime_mock_ = &runtime_mock;
    }

    static IRuntime* runtime_mock_;
};

}  // namespace detail

/**
 * \api
 * \brief Resolves given InstanceSpecifier (port name in the model) to a collection of InstanceIdentifiers via
 * manifest lookup.
 *
 * \param modelName (name of the SWC port)
 * \return container with InstanceIdentifiers
 * \requirement SWS_CM_00118
 */
score::Result<score::mw::com::InstanceIdentifierContainer> ResolveInstanceIDs(const InstanceSpecifier model_name);

/**
 * \api
 * \brief Initializes mw::com subsystem with the given configuration referenced in the command-line options.
 * \details This call is optional for a mw::com user. Only if the mw::com configuration (json) is not located in the
 *         default manifest path, this function shall be called with the commandline option
 *         "-service_instance_manifest" pointing to the json config file to be used.
 * \attention This function shall only be called ONCE per application/process lifetime! A second call may have no
 *            effect after an internal runtime singleton has been already created/is in use!
 */
// NOLINTNEXTLINE(modernize-avoid-c-arrays):C-style array tolerated for command line arguments
void InitializeRuntime(const std::int32_t argc, score::StringLiteral argv[]);

/**
 * \api
 * \brief Initializes mw::com subsystem with the given configuration.
 * \details This call is optional for a mw::com user. Only if the mw::com configuration (json) is not located in the
 *         default manifest path, this function shall be called when the caller already has the configuration path.
 * \attention This function shall only be called ONCE per application/process lifetime! A second call may have no
 *            effect after an internal runtime singleton has been already created/is in use!
 */
void InitializeRuntime(const RuntimeConfiguration& runtime_configuration);

}  // namespace score::mw::com::runtime

#endif  // SCORE_MW_COM_RUNTIME_H
