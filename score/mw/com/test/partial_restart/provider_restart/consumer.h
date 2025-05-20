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

#ifndef PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_PROVIDER_RESTART_CONSUMER_H
#define PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_PROVIDER_RESTART_CONSUMER_H

#include "score/mw/com/test/common_test_resources/check_point_control.h"

namespace bmw::mw::com::test
{

    struct ConsumerParameters
    {
        bool create_and_run_proxy;
    };

    /// \brief Test sequence done by the consumer process in all Partial Restart ITFs (1-6)
    /// \details Depending on the test-parameter ConsumerParameters::create_and_run_proxy the implementation of this func
    ///          dispatches to two different internal functions, which implement two different sequences, which are the
    ///          Consumer sequences defined in README.md in case of ITF 1 and ITF 2.
    ///          The single entry-point to the consumer sequences can be re-used in all 6 ITFs:
    ///          ITF 1: ConsumerParameters::create_and_run_proxy = true
    ///          ITF 2: ConsumerParameters::create_and_run_proxy = false
    ///          ITF 3: ConsumerParameters::create_and_run_proxy = true
    ///          ITF 4: ConsumerParameters::create_and_run_proxy = false
    ///          ITF 5: ConsumerParameters::create_and_run_proxy = true
    ///          ITF 6: ConsumerParameters::create_and_run_proxy = true
    ///          For the consumer restart tests (ITF 5/6) the sequence of the consumer can be much simpler. I.e. already
    ///          after the consumer reached checkpoint (1) - received N samples from provider - the sequence is basically
    ///          over and the consumer either gets killed or notified to terminate. We are still re-using the same consumer
    ///          implementation with the additional steps implemented, but in ITF 5/6 these steps won't be executed.
    /// \see README.md in this directory
    /// \param check_point_control check-point-ctrl communication object to sync with the controller (get proceed triggers
    ///        from controller and send notifications to controller about reached check-point or error)
    /// \param test_stop_token stop-token connected to the overall test connected to the signal-handler set up in main().
    ///        I.e. this stop-token gets a stop-request sent, when the test infrastructure kills the test.
    /// \param argc handed over by the test/main() in case the test has been started with "-service_instance_manifest", so
    ///        that argc/argv can be used to initialize the lola/mw_com runtime with the cmdline.
    /// \param argv see argc
    /// \param test_params parameters for sequence execution.
    void DoConsumerActions(CheckPointControl &check_point_control,
                           amp::stop_token test_stop_token,
                           int argc,
                           const char **argv,
                           ConsumerParameters test_params) noexcept;

} // namespace bmw::mw::com::test

#endif // PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_CONSUMER_CRASH_CONSUMER_H
