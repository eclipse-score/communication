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
#ifndef SCORE_MW_COM_TEST_SKELETON_REOFFER_TEST_PARAMETERS_H
#define SCORE_MW_COM_TEST_SKELETON_REOFFER_TEST_PARAMETERS_H

#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace score::mw::com::test::skeleton_reoffer
{

constexpr std::size_t kNumSamplesPerOffer{5U};
constexpr std::uint32_t kFirstOfferInitialValue{1U};
constexpr std::uint32_t kSecondOfferInitialValue{6U};

constexpr std::string_view kFirstBatchReceivedShmPath{"/skeleton_reoffer_first_batch_received"};
constexpr std::string_view kStopOfferDoneShmPath{"/skeleton_reoffer_stop_offer_done"};
constexpr std::string_view kReadyForReofferShmPath{"/skeleton_reoffer_ready_for_reoffer"};
constexpr std::string_view kReofferDoneShmPath{"/skeleton_reoffer_reoffer_done"};
constexpr std::string_view kConsumerDoneShmPath{"/skeleton_reoffer_consumer_done"};

enum class Scenario : std::uint8_t
{
    kNoConnectedProxy,
    kWithConnectedProxy,
    kProxyDisconnectedBeforeReoffer
};

struct TestParameters
{
    Scenario scenario;
    std::string service_instance_manifest;
};

TestParameters ReadCommandLineArguments(int argc, const char** argv);

InstanceSpecifier GetTestInstanceSpecifier();

std::vector<std::uint32_t> CreateExpectedSamples(std::uint32_t initial_value, std::size_t num_samples);

std::optional<ProcessSynchronizer> CreateSynchronizer(std::string_view shm_path);

void WaitForNotification(std::optional<ProcessSynchronizer>& synchronizer,
                         const score::cpp::stop_token& stop_token,
                         std::string_view barrier_name);

}  // namespace score::mw::com::test::skeleton_reoffer

#endif  // SCORE_MW_COM_TEST_SKELETON_REOFFER_TEST_PARAMETERS_H
