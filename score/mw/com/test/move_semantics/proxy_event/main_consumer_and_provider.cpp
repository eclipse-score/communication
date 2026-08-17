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
#include "score/mw/com/runtime.h"

#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/move_semantics/proxy_event/consumer.h"
#include "score/mw/com/test/move_semantics/proxy_event/provider.h"
#include "score/mw/com/test/move_semantics/proxy_event/test_parameters.h"

int main(int argc, const char** argv)
{
    auto test_configuration{score::mw::com::test::ReadCommandLineArguments(argc, argv)};

    score::mw::com::test::SetupAssertHandler();
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing" << std::endl;
    }

    const auto num_send_iterations = score::mw::com::test::GetNumberOfSendIterations(test_configuration.scenario);

    std::cout << "Starting provider and consumer with scenario "
              << static_cast<std::size_t>(test_configuration.scenario) << ", number of samples to send per offer "
              << score::mw::com::test::kNumberOfSamplesToSendPerOffer << " and number of send iterations "
              << num_send_iterations << std::endl;

    // Pre-create the ProcessSynchronizer SHM objects in the main thread before launching provider and consumer.
    //
    // Both RunProvider and RunConsumer independently call CreateOrOpenObject on the same SHM paths at startup.
    // CreateOrOpenObject uses a LockFile (O_CREAT|O_EXCL) to serialise SHM creation; when both threads race,
    // the "loser" must wait up to 500 ms for the lock file to be released. On a loaded CI machine the winning
    // thread can be preempted for longer than that (while executing shm_open + ftruncate + mmap + constructor),
    // causing WaitForFreeLockFile to time out → EBUSY → std::terminate().
    //
    // Creating the SHM here first means both async tasks go through the faster OpenObject path, which holds the
    // lock file only for a brief shm_open + mmap (~1 ms) — safely within the 500 ms budget.
    const std::string kInterprocessNotificationShmPath{"/proxy_event_move_semantics_interprocess_notification"};
    const std::string kProviderWithdrawNotificationShmPath{
        "/proxy_event_move_semantics_provider_withdraw_notification"};
    auto process_sync_guard =
        score::mw::com::test::ProcessSynchronizer::CreateUniquePtr(kInterprocessNotificationShmPath);
    process_sync_guard->Reset();
    auto withdraw_sync_guard =
        score::mw::com::test::ProcessSynchronizer::CreateUniquePtr(kProviderWithdrawNotificationShmPath);
    withdraw_sync_guard->Reset();

    auto provider_future = std::async(score::mw::com::test::RunProvider,
                                      score::mw::com::test::kNumberOfSamplesToSendPerOffer,
                                      num_send_iterations,
                                      stop_source.get_token());
    auto consumer_future = std::async(score::mw::com::test::RunConsumer,
                                      test_configuration.scenario,
                                      score::mw::com::test::kNumberOfSamplesToSendPerOffer,
                                      num_send_iterations,
                                      stop_source.get_token());

    provider_future.get();
    consumer_future.get();

    return EXIT_SUCCESS;
}
