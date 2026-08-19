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
 *******************************************************************************/
#ifndef SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_APPLICATION_RUNNER_H
#define SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_APPLICATION_RUNNER_H

#include <string>

namespace score::mw::com::gateway
{

/// \brief Signal handler that requests a graceful shutdown of a running gateway application.
/// \details Registered by callers for signals such as SIGINT/SIGTERM before invoking RunGatewayApplication().
void SignalHandler(int signal) noexcept;

/// \brief Parses the gateway configuration, sets up and starts the gateway application, then blocks until a
/// shutdown is requested via SignalHandler().
/// \param config_path Path to the gateway configuration file.
void RunGatewayApplication(const std::string& config_path);

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_APPLICATION_RUNNER_H
