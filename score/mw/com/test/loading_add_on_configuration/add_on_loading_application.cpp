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

#include "score/mw/com/test/loading_add_on_configuration/add_on_loading_application.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"

#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/sample_sender_receiver.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_guard.h"
#include "score/os/utils/interprocess/interprocess_notification.h"

#include <score/assert.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace
{

std::string ParseServiceInstanceManifest(int argc, const char** argv)
{
    const std::string service_instance_manifest_name = "addon_manifest";

    auto args = score::mw::com::test::ParseCommandLineArguments(argc, argv, {{service_instance_manifest_name, ""}});
    return score::mw::com::test::GetValue<std::string>(args, service_instance_manifest_name);
}

// Named shared-memory paths for the InterprocessNotification triples used to explicitly exchange state information
// between sender and receiver across the 3 communication phases of this test. Each phase gets its own triple so
// that phases can ever interfere with one another:
//  - "offered" is notified by the skeleton once it has offered the service (the proxy must wait for this before
//    finding/subscribing);
//  - "subscribed" is notified by the proxy once it has finished subscribing (the skeleton must wait for this
//    before it starts sending, so no samples are published before there is a subscriber to receive them);
//  - "done" is notified by the proxy once it has received everything it needs (the skeleton must wait for this
//    before calling StopOfferService()).
const std::string kPhase1OfferedShmPath{"/addon_test_phase1_offered"};
const std::string kPhase1SubscribedShmPath{"/addon_test_phase1_subscribed"};
const std::string kPhase1DoneShmPath{"/addon_test_phase1_done"};
const std::string kPhase2OfferedShmPath{"/addon_test_phase2_offered"};
const std::string kPhase2SubscribedShmPath{"/addon_test_phase2_subscribed"};
const std::string kPhase2DoneShmPath{"/addon_test_phase2_done"};
const std::string kPhase3OfferedShmPath{"/addon_test_phase3_offered"};
const std::string kPhase3SubscribedShmPath{"/addon_test_phase3_subscribed"};
const std::string kPhase3DoneShmPath{"/addon_test_phase3_done"};

// All 9 shared-memory paths used by the notification exchange, shared between the sender and receiver branches.
const std::vector<std::string> kAllShmPaths{
    kPhase1OfferedShmPath,
    kPhase1SubscribedShmPath,
    kPhase1DoneShmPath,
    kPhase2OfferedShmPath,
    kPhase2SubscribedShmPath,
    kPhase2DoneShmPath,
    kPhase3OfferedShmPath,
    kPhase3SubscribedShmPath,
    kPhase3DoneShmPath,
};

// RAII helper that creates/opens a SharedMemoryObjectCreator<InterprocessNotification> for each given
// shared-memory path and keeps a SharedMemoryObjectGuard alongside it so that all objects are
// cleaned up automatically.
class InterprocessNotificationSet final
{
  public:
    explicit InterprocessNotificationSet(const std::vector<std::string>& shared_memory_paths)
    {
        for (const auto& path : shared_memory_paths)
        {
            auto creator_result = score::mw::com::test::SharedMemoryObjectCreator<
                score::os::InterprocessNotification>::CreateOrOpenObject(path);
            if (!creator_result.has_value())
            {
                valid_ = false;
                return;
            }
            auto [iterator, inserted] = creators_.try_emplace(path, std::move(creator_result).value());
            static_cast<void>(inserted);
            guards_.try_emplace(path, iterator->second);
        }
    }

    ~InterprocessNotificationSet() = default;

    InterprocessNotificationSet(const InterprocessNotificationSet&) = delete;
    InterprocessNotificationSet& operator=(const InterprocessNotificationSet&) = delete;
    InterprocessNotificationSet(InterprocessNotificationSet&&) = delete;
    InterprocessNotificationSet& operator=(InterprocessNotificationSet&&) = delete;

    bool IsValid() const noexcept
    {
        return valid_;
    }

    score::os::InterprocessNotification& Get(const std::string& shared_memory_path)
    {
        return creators_.at(shared_memory_path).GetObject();
    }

  private:
    bool valid_{true};
    // Declaration order matters here: members are destroyed in reverse declaration order, so guards_ (which
    // hold references into creators_) are destroyed first, and creators_ (the referents) are destroyed after.
    std::map<std::string, score::mw::com::test::SharedMemoryObjectCreator<score::os::InterprocessNotification>>
        creators_;
    std::map<std::string, score::mw::com::test::SharedMemoryObjectGuard<score::os::InterprocessNotification>> guards_;
};

score::mw::com::InstanceSpecifier GetInstanceSpecifier(const std::string& specifier_string)
{
    const auto instance_specifier_result = score::mw::com::InstanceSpecifier::Create(specifier_string);
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;
        std::terminate();
    }
    return instance_specifier_result.value();
}

}  // namespace

int main(int argc, const char** argv)
{
    score::mw::com::test::SetupAssertHandler();
    using Parameters = score::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{
        Parameters::MODE, Parameters::NUM_CYCLES, Parameters::CYCLE_TIME, Parameters::SERVICE_INSTANCE_MANIFEST};
    score::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto& run_parameters = test_runner.GetRunParameters();
    const auto mode = run_parameters.GetMode();
    const auto num_cycles = run_parameters.GetNumCycles();
    const auto stop_token = test_runner.GetStopToken();

    score::mw::com::test::EventSenderReceiver event_sender_receiver{};

    const auto& instance_specifier = GetInstanceSpecifier(std::string{"score/cp60/MapApiLanesStamped"});

    if (mode == "send" || mode == "skeleton")
    {
        const auto cycle_time = run_parameters.GetCycleTime();

        // Create inter process notifications used to share state between the 2 applications
        InterprocessNotificationSet notifications{kAllShmPaths};
        if (!notifications.IsValid())
        {
            std::cerr << "Sender: Failed to create/open interprocess notification objects, terminating." << std::endl;
            return EXIT_FAILURE;
        }

        // 1st send phase: Send service from original configuration
        const auto first_send_result =
            event_sender_receiver.RunAsSkeletonWithNotificationExchange(instance_specifier,
                                                                        cycle_time,
                                                                        num_cycles,
                                                                        notifications.Get(kPhase1DoneShmPath),
                                                                        notifications.Get(kPhase1OfferedShmPath),
                                                                        notifications.Get(kPhase1SubscribedShmPath),
                                                                        stop_token);

        if (first_send_result != EXIT_SUCCESS)
        {
            return first_send_result;
        }

        // Load add-on configuration
        const auto service_instance_manifest_path = ParseServiceInstanceManifest(argc, argv);
        const auto add_on_load_result = score::mw::com::runtime::InitializeRuntimeAddonConfiguration(
            score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest_path});

        if (!add_on_load_result.has_value())
        {
            std::cout << "Sender: Failed to load add-on configuration: " << add_on_load_result.error() << std::endl;
            return EXIT_FAILURE;
        }

        // After loading add-on config. 2nd send phase: Try to send initial service again
        const auto second_send_result =
            event_sender_receiver.RunAsSkeletonWithNotificationExchange(instance_specifier,
                                                                        cycle_time,
                                                                        num_cycles,
                                                                        notifications.Get(kPhase2DoneShmPath),
                                                                        notifications.Get(kPhase2OfferedShmPath),
                                                                        notifications.Get(kPhase2SubscribedShmPath),
                                                                        stop_token);

        if (second_send_result != EXIT_SUCCESS)
        {
            std::cerr << "Sender: Failed to offer initial service again after add-on configuration was loaded"
                      << std::endl;
            return EXIT_FAILURE;
        }

        const auto& addon_instance_specifier = GetInstanceSpecifier("score/cp60/MapApiLanes");
        // 3rd send phase: Try to send new service defined in add-on configuration
        const auto addon_send_result =
            event_sender_receiver.RunAsSkeletonWithNotificationExchange(addon_instance_specifier,
                                                                        cycle_time,
                                                                        num_cycles,
                                                                        notifications.Get(kPhase3DoneShmPath),
                                                                        notifications.Get(kPhase3OfferedShmPath),
                                                                        notifications.Get(kPhase3SubscribedShmPath),
                                                                        stop_token);

        if (addon_send_result != EXIT_SUCCESS)
        {
            std::cerr << "Sender: Failed to offer new add-on service" << std::endl;
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
    else if (mode == "recv" || mode == "proxy")
    {
        const auto cycle_time = run_parameters.GetCycleTime();

        // Create inter process notifications used to share state between the 2 applications
        InterprocessNotificationSet notifications{kAllShmPaths};
        if (!notifications.IsValid())
        {
            std::cerr << "Receiver: Failed to create/open interprocess notification objects, terminating." << std::endl;
            return EXIT_FAILURE;
        }

        // 1st receive phase: Try to receive service from original configuration
        const auto first_receiv_result =
            event_sender_receiver.RunAsProxyWithNotificationExchange(instance_specifier,
                                                                     cycle_time,
                                                                     num_cycles,
                                                                     notifications.Get(kPhase1OfferedShmPath),
                                                                     notifications.Get(kPhase1DoneShmPath),
                                                                     notifications.Get(kPhase1SubscribedShmPath),
                                                                     stop_token);

        if (first_receiv_result != EXIT_SUCCESS)
        {
            return first_receiv_result;
        }

        // Initial communication was successful, load add-on configuration
        const auto service_instance_manifest_path = ParseServiceInstanceManifest(argc, argv);
        const auto add_on_load_result = score::mw::com::runtime::InitializeRuntimeAddonConfiguration(
            score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest_path});

        if (!add_on_load_result.has_value())
        {
            std::cout << "Receiver: Failed to load add-on configuration on receiver side: "
                      << add_on_load_result.error() << std::endl;
            return EXIT_FAILURE;
        }

        // Add-on configuration successfully loaded. 2nd receive phase: Try to receive first service again
        const auto second_receiv_result =
            event_sender_receiver.RunAsProxyWithNotificationExchange(instance_specifier,
                                                                     cycle_time,
                                                                     num_cycles,
                                                                     notifications.Get(kPhase2OfferedShmPath),
                                                                     notifications.Get(kPhase2DoneShmPath),
                                                                     notifications.Get(kPhase2SubscribedShmPath),
                                                                     stop_token);

        if (second_receiv_result != EXIT_SUCCESS)
        {
            std::cerr << "Receiver: Failed to receive initial service again after add-on configuration was loaded"
                      << std::endl;
            return EXIT_FAILURE;
        }

        const auto& addon_instance_specifier = GetInstanceSpecifier("score/cp60/MapApiLanes");

        // 3rd receive phase: Receive service defined in add-on configuration
        const auto addon_receiv_result =
            event_sender_receiver.RunAsProxyWithNotificationExchange(addon_instance_specifier,
                                                                     cycle_time,
                                                                     num_cycles,
                                                                     notifications.Get(kPhase3OfferedShmPath),
                                                                     notifications.Get(kPhase3DoneShmPath),
                                                                     notifications.Get(kPhase3SubscribedShmPath),
                                                                     stop_token);

        if (addon_receiv_result != EXIT_SUCCESS)
        {
            std::cerr << "Receiver: Failed to receive new add-on service" << std::endl;
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    std::cerr << "Unknown mode " << mode << ", terminating." << std::endl;
    return EXIT_FAILURE;
}
