/*******************************************************************************
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

#ifndef SCORE_MW_COM_TEST_PARTIAL_RESTART_PROVIDER_RESTART_CONTROLLER_H
#define SCORE_MW_COM_TEST_PARTIAL_RESTART_PROVIDER_RESTART_CONTROLLER_H

#include <score/stop_token.hpp>

namespace score::mw::com::test
{

/// \brief This is the test sequence done by the Controller for ITF 1 - Provider normal restart - connected Proxy
/// \see README.md in this directory
/// \param test_stop_token stop-token connected to the overall test connected to the signal-handler set up in main().
///        I.e. this stop-token gets a stop-request sent, when the test infrastructure kills the test.
/// \param argc handed over by the test/main() in case the test has been started with "-service_instance_manifest", so
///        that argc/argv can be used to initialize the lola/mw_com runtime with the cmdline.
/// \param argv see argc
/// \return Either EXIT_FAILURE (failure in test sequence happened) or EXIT_SUCCESS
int DoProviderNormalRestartSubscribedProxy(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept;

/// \brief This is the test sequence done by the Controller for ITF 2 - Provider normal restart - without connected
/// Proxy \see README.md in this directory \param test_stop_token stop-token connected to the overall test connected to
/// the signal-handler set up in main().
///        I.e. this stop-token gets a stop-request sent, when the test infrastructure kills the test.
/// \param argc handed over by the test/main() in case the test has been started with "-service_instance_manifest", so
///        that argc/argv can be used to initialize the lola/mw_com runtime with the cmdline.
/// \param argv see argc
/// \return Either EXIT_FAILURE (failure in tzest sequence happened) or EXIT_SUCCESS
int DoProviderNormalRestartNoProxy(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept;

/// \brief This is the test sequence done by the Controller for ITF 3 - Provider crash restart - connected Proxy
/// \see README.md in this directory
/// \param test_stop_token stop-token connected to the overall test connected to the signal-handler set up in main().
///        I.e. this stop-token gets a stop-request sent, when the test infrastructure kills the test.
/// \param argc handed over by the test/main() in case the test has been started with "-service_instance_manifest", so
///        that argc/argv can be used to initialize the lola/mw_com runtime with the cmdline.
/// \param argv see argc
/// \return Either EXIT_FAILURE (failure in tzest sequence happened) or EXIT_SUCCESS
int DoProviderCrashRestartSubscribedProxy(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept;

/// \brief This is the test sequence done by the Controller for ITF 4 - Provider crash restart - without connected Proxy
/// \see README.md in this directory
/// \param test_stop_token stop-token connected to the overall test connected to the signal-handler set up in main().
///        I.e. this stop-token gets a stop-request sent, when the test infrastructure kills the test.
/// \param argc handed over by the test/main() in case the test has been started with "-service_instance_manifest", so
///        that argc/argv can be used to initialize the lola/mw_com runtime with the cmdline.
/// \param argv see argc
/// \return Either EXIT_FAILURE (failure in tzest sequence happened) or EXIT_SUCCESS
int DoProviderCrashRestartNoProxy(score::cpp::stop_token test_stop_token, int argc, const char** argv) noexcept;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PARTIAL_RESTART_PROVIDER_RESTART_CONTROLLER_H
