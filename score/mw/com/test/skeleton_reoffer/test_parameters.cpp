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
#include "score/mw/com/test/skeleton_reoffer/test_parameters.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace score::mw::com::test::skeleton_reoffer
{
namespace
{

constexpr std::string_view kScenarioKey{"scenario"};
constexpr std::string_view kScenarioOption{"scenario,s"};
constexpr std::string_view kServiceInstanceManifestKey{"service_instance_manifest"};
constexpr std::string_view kInstanceSpecifierPath{"test/skeleton_reoffer/TestService"};

constexpr std::string_view kNoConnectedProxyValue{"no_connected_proxy"};
constexpr std::string_view kWithConnectedProxyValue{"with_connected_proxy"};
constexpr std::string_view kProxyDisconnectedBeforeReofferValue{"proxy_disconnected_before_reoffer"};

Scenario ParseScenario(const std::string_view scenario_value)
{
    if (scenario_value == kNoConnectedProxyValue)
    {
        return Scenario::kNoConnectedProxy;
    }
    if (scenario_value == kWithConnectedProxyValue)
    {
        return Scenario::kWithConnectedProxy;
    }
    if (scenario_value == kProxyDisconnectedBeforeReofferValue)
    {
        return Scenario::kProxyDisconnectedBeforeReoffer;
    }
    FailTest("skeleton_reoffer: Invalid scenario argument: ", scenario_value);
    return Scenario::kNoConnectedProxy;
}

}  // namespace

TestParameters ReadCommandLineArguments(int argc, const char** argv)
{
    const auto args = ParseCommandLineArguments(
        argc,
        argv,
        {{std::string{kScenarioOption}, "re-offer scenario to run"},
         {std::string{kServiceInstanceManifestKey}, "path to the service instance manifest"}});

    const auto scenario_string = GetValue<std::string>(args, std::string{kScenarioKey});
    const auto scenario = ParseScenario(scenario_string);
    auto service_instance_manifest = GetValue<std::string>(args, std::string{kServiceInstanceManifestKey});

    return {scenario, std::move(service_instance_manifest)};
}

InstanceSpecifier GetTestInstanceSpecifier()
{
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierPath});
    if (!instance_specifier_result.has_value())
    {
        FailTest("skeleton_reoffer: Could not create InstanceSpecifier: ", instance_specifier_result.error());
    }
    return std::move(instance_specifier_result).value();
}

std::vector<std::uint32_t> CreateExpectedSamples(const std::uint32_t initial_value, const std::size_t num_samples)
{
    std::vector<std::uint32_t> samples{};
    samples.reserve(num_samples);
    for (std::size_t i = 0U; i < num_samples; ++i)
    {
        samples.push_back(initial_value + static_cast<std::uint32_t>(i));
    }
    return samples;
}

std::optional<ProcessSynchronizer> CreateSynchronizer(const std::string_view shm_path)
{
    return ProcessSynchronizer::Create(std::string{shm_path});
}

void WaitForNotification(std::optional<ProcessSynchronizer>& synchronizer,
                         const score::cpp::stop_token& stop_token,
                         const std::string_view barrier_name)
{
    if (!synchronizer->WaitWithAbort(stop_token))
    {
        FailTest("skeleton_reoffer: Waiting for barrier ", barrier_name, " was aborted by stop_token");
    }
}

}  // namespace score::mw::com::test::skeleton_reoffer
