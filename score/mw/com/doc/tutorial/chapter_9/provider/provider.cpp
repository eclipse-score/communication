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
#include "../tire_pressure_extended_service.h"
#include "score/mw/com/types.h"

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <thread>

using score::mw::com::tutorial::Tire;
using score::mw::com::tutorial::TirePressureExtendedSkeleton;
using score::mw::com::tutorial::TirePressureThreshold;

// In this chapter the provider offers a single instance of the TirePressureExtendedService.
constexpr std::string_view kInstanceSpecifierString{"MyTirePressureExtendedServiceInstance"};

// The initial pressure (in bar) each per-wheel field is set to *before* the service is offered. A field always has a
// current value, so the provider must supply one for every field before it may offer the service.
constexpr float kInitialTirePressure{2.0F};

// The initial value of the "tire_pressure_thresholds" field (in bar).
constexpr float kInitialLowerThreshold{2.0F};
constexpr float kInitialUpperThreshold{3.0F};

// The set-handler clamps a consumer-requested threshold band into these bounds (in bar).
constexpr float kLowerThresholdMin{1.5F};
constexpr float kLowerThresholdMax{2.5F};
constexpr float kUpperThresholdMin{2.5F};
constexpr float kUpperThresholdMax{3.5F};

// After the service has been offered the provider waits this long before it starts updating the fields.
constexpr std::chrono::seconds kInitialUpdateDelay{20};

// The field updates happen at a randomized cadence between kMinUpdateDelay and kMaxUpdateDelay.
constexpr std::chrono::milliseconds kMinUpdateDelay{100};
constexpr std::chrono::milliseconds kMaxUpdateDelay{3000};

// The randomized field values are drawn from the interval [kMinTirePressure, kMaxTirePressure].
constexpr float kMinTirePressure{1.2F};
constexpr float kMaxTirePressure{4.0F};

static std::atomic<bool> g_running{true};

// The current threshold band. It is the provider's local mirror of the "tire_pressure_thresholds" field value: it is
// initialized before the service is offered and updated by the set-handler whenever a consumer calls Set() on the
// field. The main loop reads it to decide whether a tire pressure has left the band.
static std::mutex g_thresholds_mutex{};
static TirePressureThreshold g_current_thresholds{kInitialLowerThreshold, kInitialUpperThreshold};

static void SignalHandler(int /*signum*/)
{
    std::cout << "TirePressureExtended service provider caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

// All four per-wheel fields have the same (skeleton) field type. We deduce it from one of the skeleton's field members
// instead of naming the internal (impl-namespace) type directly - user applications must not reference
// score::mw::com::impl types.
using TirePressureField = decltype(TirePressureExtendedSkeleton::tire_pressure_front_left);

// Updates a single tire pressure field with the given value and returns the value it was set to.
static float UpdateField(TirePressureField& field, const std::string_view field_name, const float value)
{
    const auto update_result = field.Update(value);
    if (!update_result)
    {
        std::cerr << "Failed to update field \"" << field_name << "\": " << update_result.error() << std::endl;
    }
    else
    {
        std::cout << "  " << field_name << " = " << value << " bar" << std::endl;
    }
    return value;
}

// Clamps a single threshold value into [min, max] and logs an error message when an adjustment was necessary.
static float ClampThreshold(const std::string_view threshold_name,
                            const float value,
                            const float min_value,
                            const float max_value)
{
    if (value < min_value)
    {
        std::cerr << "Set handler: requested " << threshold_name << " (" << value << " bar) is below " << min_value
                  << " bar. Adjusting to " << min_value << " bar." << std::endl;
        return min_value;
    }
    if (value > max_value)
    {
        std::cerr << "Set handler: requested " << threshold_name << " (" << value << " bar) is above " << max_value
                  << " bar. Adjusting to " << max_value << " bar." << std::endl;
        return max_value;
    }
    return value;
}

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    auto service_instance_result = TirePressureExtendedSkeleton::Create(instance_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_instance_result.has_value(),
                                                "Failed to create TirePressureExtendedSkeleton instance!");
    auto service_instance = std::move(service_instance_result).value();

    // Register the set-handler for the "tire_pressure_thresholds" field. Because Get()/Set() on a field are just
    // methods, the handler has to be registered *before* the service is offered (otherwise OfferService() fails). The
    // handler receives the consumer-requested value by reference, validates/clamps it in place and thereby determines
    // the value that actually becomes the new field value (the middleware updates the field with the value the handler
    // leaves behind). We additionally mirror the accepted value into g_current_thresholds so the update loop can use
    // it.
    const auto set_handler_result =
        service_instance.tire_pressure_thresholds.RegisterSetHandler([](TirePressureThreshold& value) noexcept {
            value.lower_threshold =
                ClampThreshold("lower_threshold", value.lower_threshold, kLowerThresholdMin, kLowerThresholdMax);
            value.upper_threshold =
                ClampThreshold("upper_threshold", value.upper_threshold, kUpperThresholdMin, kUpperThresholdMax);

            {
                std::lock_guard<std::mutex> lock{g_thresholds_mutex};
                g_current_thresholds = value;
            }
            std::cout << "tire_pressure_thresholds set to [lower: " << value.lower_threshold
                      << " bar, upper: " << value.upper_threshold << " bar]" << std::endl;
        });
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(set_handler_result.has_value(),
                                                "Failed to register set handler for 'tire_pressure_thresholds'!");

    // A field always has a current value. Therefore, BEFORE the provider may offer the service, it has to supply an
    // initial value for *each* field via Field::Update(). Omitting this for any field would make the OfferService()
    // call fail.
    std::cout << "Setting initial field values (before offering the service):" << std::endl;
    UpdateField(service_instance.tire_pressure_front_left, "tire_pressure_front_left", kInitialTirePressure);
    UpdateField(service_instance.tire_pressure_front_right, "tire_pressure_front_right", kInitialTirePressure);
    UpdateField(service_instance.tire_pressure_rear_left, "tire_pressure_rear_left", kInitialTirePressure);
    UpdateField(service_instance.tire_pressure_rear_right, "tire_pressure_rear_right", kInitialTirePressure);

    const TirePressureThreshold initial_thresholds{kInitialLowerThreshold, kInitialUpperThreshold};
    const auto thresholds_update_result = service_instance.tire_pressure_thresholds.Update(initial_thresholds);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(thresholds_update_result.has_value(),
                                                "Failed to set initial 'tire_pressure_thresholds' value!");
    std::cout << "  tire_pressure_thresholds = [lower: " << initial_thresholds.lower_threshold
              << " bar, upper: " << initial_thresholds.upper_threshold << " bar]" << std::endl;

    auto offer_result = service_instance.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(),
                                                "Failed to offer TirePressureExtendedSkeleton instance!");
    std::cout << "OfferService: TirePressureExtended service instance is now offered." << std::endl;

    // Give a consumer time to discover the service before we start updating the tire pressures.
    std::cout << "Waiting " << kInitialUpdateDelay.count() << " s before updating the fields ..." << std::endl;
    std::this_thread::sleep_for(kInitialUpdateDelay);

    // Random number generators for the randomized update cadence and the randomized field values.
    std::mt19937 random_engine{std::random_device{}()};
    std::uniform_int_distribution<std::chrono::milliseconds::rep> delay_distribution{kMinUpdateDelay.count(),
                                                                                     kMaxUpdateDelay.count()};
    std::uniform_real_distribution<float> pressure_distribution{kMinTirePressure, kMaxTirePressure};

    // The four per-wheel fields together with their name and the Tire value carried by a warning event for that wheel.
    const std::array<std::tuple<TirePressureField&, std::string_view, Tire>, 4U> tires{{
        {service_instance.tire_pressure_front_left, "tire_pressure_front_left", Tire::front_left},
        {service_instance.tire_pressure_front_right, "tire_pressure_front_right", Tire::front_right},
        {service_instance.tire_pressure_rear_left, "tire_pressure_rear_left", Tire::rear_left},
        {service_instance.tire_pressure_rear_right, "tire_pressure_rear_right", Tire::rear_right},
    }};

    while (g_running.load(std::memory_order_relaxed))
    {
        // Wait a randomized amount of time before updating the fields again.
        const std::chrono::milliseconds update_delay{delay_distribution(random_engine)};
        std::cout << "Next tire pressure update in " << update_delay.count() << " ms." << std::endl;
        std::this_thread::sleep_for(update_delay);

        if (!g_running.load(std::memory_order_relaxed))
        {
            break;
        }

        std::cout << "Updating tire pressures:" << std::endl;
        std::array<float, 4U> new_pressures{};
        for (std::size_t index = 0U; index < tires.size(); ++index)
        {
            auto& [field, field_name, tire] = tires[index];
            new_pressures[index] = UpdateField(field, field_name, pressure_distribution(random_engine));
        }

        // Read the current threshold band and send a "tire_pressure_warning" event for every tire whose new pressure
        // has left it. Multiple warnings may be sent after a single update cycle.
        TirePressureThreshold thresholds{};
        {
            std::lock_guard<std::mutex> lock{g_thresholds_mutex};
            thresholds = g_current_thresholds;
        }

        for (std::size_t index = 0U; index < tires.size(); ++index)
        {
            const auto& [field, field_name, tire] = tires[index];
            const float pressure = new_pressures[index];
            if ((pressure < thresholds.lower_threshold) || (pressure > thresholds.upper_threshold))
            {
                std::cout << "  '" << field_name << "' (" << pressure
                          << " bar) is outside the threshold band [lower: " << thresholds.lower_threshold
                          << " bar, upper: " << thresholds.upper_threshold << " bar]. Sending tire_pressure_warning."
                          << std::endl;
                const auto send_result = service_instance.tire_pressure_warning.Send(tire);
                if (!send_result)
                {
                    std::cerr << "Failed to send tire_pressure_warning for '" << field_name
                              << "': " << send_result.error() << std::endl;
                }
            }
        }
    }
    std::cout << "TirePressureExtended service provider going down." << std::endl;
}
