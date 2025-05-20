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

#include "score/mw/com/test/reserving_skeleton_slots/reserving_skeleton_slots_application.h"

#include "score/mw/com/impl/configuration/config_parser.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/test/common_test_resources/sample_sender_receiver.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"

#include <future>
#include <string>
#include <utility>
#include <vector>

namespace
{

    using InstanceSpecifier = bmw::mw::com::impl::InstanceSpecifier;

    std::uint16_t GetNumSkeletonSlotsFromConfig(const std::string &service_instance_manifest_path,
                                                const InstanceSpecifier instance_specifier)
    {
        const auto configuration = bmw::mw::com::impl::configuration::Parse(service_instance_manifest_path);

        const auto deployment = configuration.GetServiceInstances().at(instance_specifier);
        const auto lola_binding = amp::get<bmw::mw::com::impl::LolaServiceInstanceDeployment>(deployment.bindingInfo_);

        std::string event_name{"map_api_lanes_stamped"};
        const auto lola_event_instance = lola_binding.events_.at(event_name);

        const auto num_skeleton_slots = lola_event_instance.GetNumberOfSampleSlots().value();

        return num_skeleton_slots;
    }

    std::vector<std::future<int>> RunTest(bmw::mw::com::test::EventSenderReceiver &event_sender_receiver,
                                          const InstanceSpecifier &instance_specifier,
                                          std::uint16_t num_skeleton_slots,
                                          std::uint16_t num_proxy_slots,
                                          const amp::stop_source &stop_source,
                                          const amp::stop_token &stop_token)
    {
        std::vector<std::future<int>> future_return_values;
        auto skeleton_return_value = std::async(std::launch::async,
                                                &bmw::mw::com::test::EventSenderReceiver::RunAsSkeletonCheckEventSlots,
                                                &event_sender_receiver,
                                                instance_specifier,
                                                num_skeleton_slots,
                                                stop_source);
        future_return_values.push_back(std::move(skeleton_return_value));

        auto proxy_return_value = std::async(std::launch::async,
                                             &bmw::mw::com::test::EventSenderReceiver::RunAsProxyCheckEventSlots,
                                             &event_sender_receiver,
                                             instance_specifier,
                                             num_proxy_slots,
                                             stop_token);
        future_return_values.push_back(std::move(proxy_return_value));

        return future_return_values;
    }

    bool WaitForAsyncTestResultsFailureTest(std::vector<std::future<int>> &future_return_values)
    {
        auto &skeleton_future_value = future_return_values[0];
        auto &proxy_future_value = future_return_values[1];

        const auto skeleton_return_value = skeleton_future_value.get();
        const auto proxy_return_value = proxy_future_value.get();

        const bool failing_test_success = (!skeleton_return_value && proxy_return_value);

        return failing_test_success;
    }

} // namespace

/**
 * Integration test to test code requirement : "A skeleton event shall reserve the exact number of slots as
 * specified in the configuration."
 *
 * This test runs in 3 modes:
 *      - "passing" mode: the number of skeleton slots passed to the test is the same as that specified in the
 *        configuration.
 *      - "failing_extra_slots": the number of skeleton slots passed to the test is larger than that specified in the
 *        configuration.
 *      - "failing_less_slots" mode: the number of skeleton slots passed to the test is less than that specified in the
 *        configuration.
 *
 * The tests will set up a skeleton and proxy within the same process. The skeleton will wait for the proxy to
 * initialise and set up its handler for receiving the skeleton events. The skeleton will then publish an event, the
 * proxy will receive the event and push it into a vector and then notify the skeleton that it is finished. The skeleton
 * will then publish the next event. This ensures that the proxy receives every event that the skeleton publishes.
 *
 * Since the SamplePtr received by the proxy is stored in a persistant vector, the ref count for each slot that has been
 * allocated will always be 1. Therefore, once all slots have been allocated, the skeleton should not be able to
 * allocate any more slots.
 */
int main(int argc, const char **argv)
{
    using Parameters = bmw::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{Parameters::SERVICE_INSTANCE_MANIFEST, Parameters::MODE};
    bmw::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto &run_parameters = test_runner.GetRunParameters();
    auto stop_source = test_runner.GetStopSource();
    const auto stop_token = test_runner.GetStopToken();
    const auto service_instance_manifest_path = run_parameters.GetServiceInstanceManifest();
    const auto mode = run_parameters.GetMode();

    bmw::mw::com::test::EventSenderReceiver event_sender_receiver;

    const auto instance_specifier_result = InstanceSpecifier::Create("xpad/cp60/MapApiLanesStamped");
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Could not create instance specifier, terminating." << std::endl;
        return -1;
    }
    const auto &instance_specifier = instance_specifier_result.value();

    const auto num_skeleton_slots = GetNumSkeletonSlotsFromConfig(service_instance_manifest_path, instance_specifier);

    if (mode == "passing")
    {
        std::cout << "Running test: passing" << std::endl;
        auto passing_test_future_return_values = RunTest(
            event_sender_receiver, instance_specifier, num_skeleton_slots, num_skeleton_slots, stop_source, stop_token);

        // Wait for all threads to finish and check that they finished safely
        const auto passing_test_return_code =
            bmw::mw::com::test::SctfTestRunner::WaitForAsyncTestResults(passing_test_future_return_values);
        std::cout << "passing test: " << (passing_test_return_code ? "Failed" : "Passed") << std::endl;

        return passing_test_return_code;
    }
    else if (mode == "failing_extra_slots")
    {
        std::cout << "Running test: failing_extra_slots" << std::endl;
        auto failing_test_future_return_values = RunTest(event_sender_receiver,
                                                         instance_specifier,
                                                         static_cast<std::uint16_t>(num_skeleton_slots + 1U),
                                                         num_skeleton_slots,
                                                         stop_source,
                                                         stop_token);

        // For failing test, skeleton should return an error value and proxy should return a success value.
        const auto failing_test_return_code = WaitForAsyncTestResultsFailureTest(failing_test_future_return_values);
        std::cout << "failing_extra_slots test: " << (failing_test_return_code ? "Failed" : "Passed") << std::endl;

        return failing_test_return_code;
    }
    else if (mode == "failing_less_slots")
    {
        std::cout << "Running test: failing_less_slots" << std::endl;
        auto failing_test_future_return_values = RunTest(event_sender_receiver,
                                                         instance_specifier,
                                                         static_cast<std::uint16_t>(num_skeleton_slots - 1U),
                                                         num_skeleton_slots,
                                                         stop_source,
                                                         stop_token);

        // For failing test, skeleton should return an error value and proxy should return a success value.
        const auto failing_test_return_code = WaitForAsyncTestResultsFailureTest(failing_test_future_return_values);
        std::cout << "failing_less_slots test: " << (failing_test_return_code ? "Failed" : "Passed") << std::endl;

        return failing_test_return_code;
    }
    else
    {
        std::cerr << "Unknown mode " << mode << ", terminating." << std::endl;
        return EXIT_FAILURE;
    }
}
