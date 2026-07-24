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
#include "score/mw/com/doc/tutorial/chapter_9/tire_pressure_extended_service.h"
#include "score/mw/com/types.h"

#include "score/json/json_writer.h"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

using score::mw::com::tutorial::Tire;
using score::mw::com::tutorial::TirePressureExtendedProxy;
using score::mw::com::tutorial::TirePressureThreshold;

// The maximum number of tire_pressure_warning samples the consumer may hold at a time. It is passed to Subscribe() and
// used as the upper bound in the GetNewSamples() calls. As there are four tires, at most four warnings can be produced
// by a single provider update cycle.
constexpr std::size_t kMaxSampleCount{4U};

// The step by which the consumer widens the threshold band in reaction to a warning (in bar).
constexpr float kThresholdAdjustStep{0.1F};

// The consumer is purely event-driven: the main thread just blocks on this condition variable until a termination
// signal (SIGINT/SIGTERM) requests the shut down. The signal handler sets the flag and notifies the condition variable.
static std::mutex g_shutdown_mutex{};
static std::condition_variable g_shutdown_cv{};
static std::atomic<bool> g_shutdown_requested{false};

static void SignalHandler(int /*signum*/)
{
    g_shutdown_requested.store(true, std::memory_order_relaxed);
    g_shutdown_cv.notify_one();
}

// All four per-wheel fields have the same (proxy) field type. We deduce it from one of the proxy's field members
// instead of naming the internal (impl-namespace) type directly - user applications must not reference
// score::mw::com::impl types.
using TirePressureField = decltype(TirePressureExtendedProxy::tire_pressure_front_left);

namespace
{

// Returns a binding-independent, human-readable description of the service instance a handle refers to. The only
// portable (public API) way to obtain a representation of the instance id from a handle is:
//   HandleType::GetInstanceId() -> ServiceInstanceId::Serialize() -> score::json::Object
std::string DescribeInstanceId(const TirePressureExtendedProxy::HandleType& handle)
{
    score::json::JsonWriter json_writer{};
    auto serialized_instance_id = json_writer.ToBuffer(handle.GetInstanceId().Serialize());
    if (!serialized_instance_id.has_value())
    {
        return std::string{"<unknown>"};
    }

    const std::string pretty_printed = std::move(serialized_instance_id).value();
    std::string compact{};
    compact.reserve(pretty_printed.size());
    bool previous_was_space{false};
    for (const char character : pretty_printed)
    {
        const bool is_space = (character == ' ') || (character == '\n') || (character == '\r') || (character == '\t');
        if (is_space)
        {
            if (!previous_was_space)
            {
                compact.push_back(' ');
            }
        }
        else
        {
            compact.push_back(character);
        }
        previous_was_space = is_space;
    }
    return compact;
}

// Human-readable name of a tire.
const char* TireName(const Tire tire)
{
    switch (tire)
    {
        case Tire::front_left:
            return "tire_pressure_front_left";
        case Tire::front_right:
            return "tire_pressure_front_right";
        case Tire::rear_left:
            return "tire_pressure_rear_left";
        case Tire::rear_right:
            return "tire_pressure_rear_right";
    }
    return "<unknown>";
}

// Maps a Tire value to the proxy field carrying that tire's pressure.
TirePressureField& GetTireField(TirePressureExtendedProxy& proxy, const Tire tire)
{
    switch (tire)
    {
        case Tire::front_left:
            return proxy.tire_pressure_front_left;
        case Tire::front_right:
            return proxy.tire_pressure_front_right;
        case Tire::rear_left:
            return proxy.tire_pressure_rear_left;
        case Tire::rear_right:
            return proxy.tire_pressure_rear_right;
    }
    return proxy.tire_pressure_front_left;
}

// Reacts to a single tire_pressure_warning for the given tire: it reads (via the field getters) the current pressure of
// the affected tire and the current threshold band, logs both, and then widens the band in the direction the pressure
// left it (lower_threshold - 0.1 bar if the pressure was below the band, upper_threshold + 0.1 bar if it was above).
void HandleSingleWarning(TirePressureExtendedProxy& proxy, const Tire tire)
{
    // Read the current pressure of the affected tire via the field's Get() (request/response, not a subscription).
    auto pressure_result = GetTireField(proxy, tire).Get();
    if (!pressure_result.has_value())
    {
        std::cerr << "Failed to Get() pressure of '" << TireName(tire) << "': " << pressure_result.error() << std::endl;
        return;
    }
    const float pressure = *(pressure_result.value());

    // Read the current threshold band via the field's Get().
    auto thresholds_result = proxy.tire_pressure_thresholds.Get();
    if (!thresholds_result.has_value())
    {
        std::cerr << "Failed to Get() tire_pressure_thresholds: " << thresholds_result.error() << std::endl;
        return;
    }
    const TirePressureThreshold thresholds = *(thresholds_result.value());

    std::cout << "Received tire_pressure_warning for '" << TireName(tire) << "': current pressure = " << pressure
              << " bar; current thresholds = [lower: " << thresholds.lower_threshold
              << " bar, upper: " << thresholds.upper_threshold << " bar]." << std::endl;

    // Widen the band in the direction the pressure left it.
    TirePressureThreshold new_thresholds = thresholds;
    if (pressure < thresholds.lower_threshold)
    {
        new_thresholds.lower_threshold -= kThresholdAdjustStep;
    }
    else if (pressure > thresholds.upper_threshold)
    {
        new_thresholds.upper_threshold += kThresholdAdjustStep;
    }
    else
    {
        // The pressure is within the band - nothing to adjust.
        return;
    }

    auto set_result = proxy.tire_pressure_thresholds.Set(new_thresholds);
    if (!set_result.has_value())
    {
        std::cerr << "Failed to Set() tire_pressure_thresholds: " << set_result.error() << std::endl;
        return;
    }
    // The set operation returns the value the provider actually accepted (it may have clamped our request).
    const TirePressureThreshold accepted = *(set_result.value());
    std::cout << "  Adjusted tire_pressure_thresholds to [lower: " << accepted.lower_threshold
              << " bar, upper: " << accepted.upper_threshold << " bar]." << std::endl;
}

// Invoked (on an internal middleware thread) whenever new tire_pressure_warning samples have been received. It drains
// all new samples and reacts to each of them.
void HandleWarnings(TirePressureExtendedProxy& proxy)
{
    auto get_new_samples_result = proxy.tire_pressure_warning.GetNewSamples(
        [&proxy](auto&& sample) {
            HandleSingleWarning(proxy, *sample.Get());
        },
        kMaxSampleCount);
    if (!get_new_samples_result)
    {
        std::cerr << "Failed to get new tire_pressure_warning samples: " << get_new_samples_result.error() << std::endl;
    }
}

}  // namespace

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier =
        score::mw::com::InstanceSpecifier::Create(std::string{"MyTirePressureExtendedServiceInstance"});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    // Stable storage for the proxy of the discovered service instance. It is populated by the find-service handler and,
    // from then on, only ever accessed by the (event-driven) receive handler. It lives for the whole lifetime of the
    // application.
    std::optional<TirePressureExtendedProxy> proxy{};

    // This handler is invoked asynchronously as soon as the searched service instance becomes available. It creates the
    // proxy, subscribes to the tire_pressure_warning event and registers a receive handler for it. Because it only
    // needs a single proxy, it stops the discovery right away. Any unrecoverable set-up error terminates the
    // application.
    auto find_service_handler =
        [&proxy](score::mw::com::ServiceHandleContainer<TirePressureExtendedProxy::HandleType> handles,
                 score::mw::com::FindServiceHandle find_service_handle) noexcept {
            score::cpp::ignore = TirePressureExtendedProxy::StopFindService(find_service_handle);

            const auto& handle = handles.front();
            std::cout << "Found TirePressureExtended service instance (instance id: " << DescribeInstanceId(handle)
                      << "). Creating proxy." << std::endl;

            auto proxy_result = TirePressureExtendedProxy::Create(handle);
            if (!proxy_result)
            {
                std::cerr << "Failed to create proxy: " << proxy_result.error() << ". Terminating." << std::endl;
                std::exit(EXIT_FAILURE);
            }
            // Store the proxy at its final, stable location *before* registering the receive handler, so that the
            // handler can safely refer to it.
            proxy.emplace(std::move(proxy_result).value());
            auto* const proxy_ptr = &proxy.value();

            // Register the receive handler for the tire_pressure_warning event *before* subscribing. The capture is
            // kept small (a single pointer) so that it fits into the score::cpp::callback storage of an
            // EventReceiveHandler.
            const auto set_receive_handler_result =
                proxy_ptr->tire_pressure_warning.SetReceiveHandler([proxy_ptr]() noexcept {
                    HandleWarnings(*proxy_ptr);
                });
            if (!set_receive_handler_result)
            {
                std::cerr << "Failed to set receive handler for 'tire_pressure_warning': "
                          << set_receive_handler_result.error() << ". Terminating." << std::endl;
                std::exit(EXIT_FAILURE);
            }

            const auto subscribe_result = proxy_ptr->tire_pressure_warning.Subscribe(kMaxSampleCount);
            if (!subscribe_result)
            {
                std::cerr << "Failed to subscribe to 'tire_pressure_warning': " << subscribe_result.error()
                          << ". Terminating." << std::endl;
                std::exit(EXIT_FAILURE);
            }
            std::cout << "Subscribed to 'tire_pressure_warning'. Waiting for warnings ..." << std::endl;
        };

    auto find_service_handle_result =
        TirePressureExtendedProxy::StartFindService(std::move(find_service_handler), instance_specifier.value());
    if (!find_service_handle_result)
    {
        std::cerr << "Failed to start service discovery: " << find_service_handle_result.error() << std::endl;
        return EXIT_FAILURE;
    }

    // There is nothing to poll here: all reception happens event-driven in the receive handler (on an internal
    // middleware thread). The main thread simply blocks until a termination signal requests the shut down.
    {
        std::unique_lock<std::mutex> lock{g_shutdown_mutex};
        g_shutdown_cv.wait(lock, [] {
            return g_shutdown_requested.load(std::memory_order_relaxed);
        });
    }

    // Stop the asynchronous service discovery before shutting down. This is a no-op if the find-service handler already
    // stopped it (i.e. if the service instance had been found).
    score::cpp::ignore = TirePressureExtendedProxy::StopFindService(find_service_handle_result.value());
    std::cout << "TirePressureExtended service consumer going down." << std::endl;

    return EXIT_SUCCESS;
}
