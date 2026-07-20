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

// Bidirectional discovery — application B.
// Each application is both a provider of its own service and a consumer of the other's.
//
// Two facts determine the init-phase ordering required for heap-free operation:
//   1. Proxy::Create() allocates heap memory and must succeed before forbid_heap().
//   2. OfferService() registers the service in the service-discovery registry. A side
//      that has not called OfferService() is not visible to service discovery, so
//      FindService() and the StartFindService() callback cannot find it and
//      Proxy::Create() cannot proceed. OfferService() also creates the shared-memory
//      region that the proxy attaches to.
//
// Each application therefore calls OfferService() before StartFindService(), making its
// own service available immediately so the other application can create a proxy against it.
// Both applications can start in any order.

#include "common/tire_pressure_service.h"
#include "heap_check/heap_check.h"

#include "score/concurrency/notification.h"
#include "score/mw/log/logging.h"
#include "score/stop_token.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <thread>

using score::mw::com::tutorial::TirePressureProxy;
using score::mw::com::tutorial::TirePressureSkeleton;

namespace
{

constexpr std::chrono::seconds kDiscoveryTimeout{5};
constexpr std::uint32_t kNumIterations{20U};
constexpr std::chrono::milliseconds kCycleTime{50};
constexpr std::size_t kMaxSamples{16U};

}  // namespace

int main()
{
    score::mw::log::LogInfo("BiB") << "Starting bidirectional_discovery/application_b";

    // ---- INIT PHASE ----

    score::Result<score::mw::com::InstanceSpecifier> own_spec_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/tutorial/bidirectional_discovery/application_b"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(own_spec_result.has_value(), "Failed to create InstanceSpecifier!");

    score::Result<TirePressureSkeleton> skeleton_result = TirePressureSkeleton::Create(own_spec_result.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton_result.has_value(), "Failed to create TirePressureSkeleton!");
    TirePressureSkeleton& skeleton = skeleton_result.value();

    score::Result<void> init_update_result = skeleton.tire_pressure_front_left.Update(0.0F);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(init_update_result.has_value(),
                                                "Failed to set tire_pressure_front_left!");

    // Offer before starting discovery. OfferService() registers this application's service
    // in the service-discovery registry and creates the shared-memory region that
    // application A needs for Proxy::Create(). Both applications can start in any order.
    score::Result<void> offer_result = skeleton.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(), "Failed to offer TirePressureSkeleton!");

    score::Result<score::mw::com::InstanceSpecifier> remote_spec_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"/tutorial/bidirectional_discovery/application_a"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(remote_spec_result.has_value(),
                                                "Failed to create remote InstanceSpecifier!");

    std::optional<TirePressureProxy> remote_proxy{};
    score::concurrency::Notification proxy_ready{};
    score::cpp::stop_source stop_source{};

    score::Result<score::mw::com::FindServiceHandle> find_service_handle_result = TirePressureProxy::StartFindService(
        [&remote_proxy, &proxy_ready](score::mw::com::ServiceHandleContainer<TirePressureProxy::HandleType> handles,
                                      score::mw::com::FindServiceHandle /*fsh*/) {
            if (handles.empty())
            {
                return;
            }
            score::Result<TirePressureProxy> proxy_result = TirePressureProxy::Create(handles.front());
            if (!proxy_result.has_value())
            {
                score::mw::log::LogError("BiB") << "TirePressureProxy::Create failed in discovery callback";
                proxy_ready.notify();
                return;
            }
            remote_proxy.emplace(std::move(proxy_result.value()));
            proxy_ready.notify();
        },
        remote_spec_result.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(find_service_handle_result.has_value(),
                                                "Failed to start service discovery!");

    // Wait for application A's Proxy::Create() to complete before forbid_heap().
    // Proxy::Create() allocates heap memory and can only succeed after application A
    // has called OfferService() and registered itself in service discovery.
    bool notified = proxy_ready.waitForWithAbort(kDiscoveryTimeout, stop_source.get_token());

    score::Result<void> stop_result = TirePressureProxy::StopFindService(find_service_handle_result.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(stop_result.has_value(), "Failed to stop service discovery!");

    if (!notified || !remote_proxy.has_value())
    {
        score::mw::log::LogError("BiB") << "Application A not found within timeout — is application_a running?";
        return EXIT_FAILURE;
    }

    TirePressureProxy& proxy = remote_proxy.value();

    score::Result<void> subscribe_result = proxy.tire_pressure_update.Subscribe(kMaxSamples);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(subscribe_result.has_value(),
                                                "Failed to subscribe to tire_pressure_update!");

    // ---- OPERATIONAL PHASE (see README.rst for heap behavior of each API) ----
    score::mw::log::LogInfo("BiB") << "Entering operational phase (heap forbidden)";
    heap_check::forbid_heap();

    for (std::uint32_t i = 0U; i < kNumIterations; ++i)
    {
        score::Result<score::mw::com::SampleAllocateePtr<float>> sample_result =
            skeleton.tire_pressure_update.Allocate();
        if (!sample_result.has_value())
        {
            score::mw::log::LogError("BiB") << "tire_pressure_update.Allocate failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        *sample_result.value() = static_cast<float>(i) * 0.1F;

        score::Result<void> send_result = skeleton.tire_pressure_update.Send(std::move(sample_result.value()));
        if (!send_result.has_value())
        {
            score::mw::log::LogError("BiB") << "tire_pressure_update.Send failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        score::Result<std::size_t> recv_result = proxy.tire_pressure_update.GetNewSamples(
            [](score::mw::com::SamplePtr<float> sample) noexcept {
                static_cast<void>(*sample);
            },
            kMaxSamples);
        if (!recv_result.has_value())
        {
            score::mw::log::LogError("BiB") << "tire_pressure_update.GetNewSamples failed";
            heap_check::allow_heap();
            return EXIT_FAILURE;
        }

        std::this_thread::sleep_for(kCycleTime);
    }

    // ---- CLEANUP ----
    heap_check::allow_heap();
    score::mw::log::LogInfo("BiB") << "Operational phase complete (" << kNumIterations << " iterations)";

    proxy.tire_pressure_update.Unsubscribe();
    skeleton.StopOfferService();

    score::mw::log::LogInfo("BiB") << "Completed successfully";
    return 0;
}
