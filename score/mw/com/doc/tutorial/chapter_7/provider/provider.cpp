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
#include "../tire_pressure_service.h"
#include "score/mw/com/types.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <thread>

// In this chapter the provider offers a single instance of the TirePressureService.
constexpr std::string_view kInstanceSpecifierString{"MyTirePressureServiceInstance"};

// The initial value each field is set to *before* the service is offered. A field always has a current value, so the
// provider must supply one for every field before it may offer the service.
constexpr float kInitialTirePressure{2.0F};

// After the service has been offered the provider waits this long before it starts updating the fields.
constexpr std::chrono::seconds kInitialUpdateDelay{20};

// The field updates happen at a randomized cadence between kMinUpdateDelay and kMaxUpdateDelay.
constexpr std::chrono::milliseconds kMinUpdateDelay{100};
constexpr std::chrono::milliseconds kMaxUpdateDelay{3000};

// The randomized field values are drawn from the interval [kMinTirePressure, kMaxTirePressure].
constexpr float kMinTirePressure{2.0F};
constexpr float kMaxTirePressure{2.5F};

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    std::cout << "TirePressure service provider caught signal." << std::endl;
    g_running.store(false, std::memory_order_relaxed);
}

using TirePressureSkeleton = score::mw::com::AsSkeleton<score::mw::com::tutorial::TirePressureInterface>;
// Deduce the concrete skeleton-field type from one of the skeleton's field members instead of naming the internal
// (impl-namespace) type directly - user applications must not reference score::mw::com::impl types. All four fields
// have the same type, so any of them can be used here.
using TirePressureField = decltype(TirePressureSkeleton::tire_pressure_front_left);

// Updates a single field with the given value. In contrast to an event, a field is updated via Field::Update(). We use
// the Update(const FieldType&) overload here: it takes the value by const-reference and lets the middleware copy it.
// This is the overload that must also be used to set the *initial* value of a field before OfferService().
static void UpdateField(TirePressureField& field, const std::string_view field_name, const float value)
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
}

int main()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto instance_specifier = score::mw::com::InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_specifier.has_value(), "Failed to create InstanceSpecifier!");

    auto service_instance_result = TirePressureSkeleton::Create(instance_specifier.value());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_instance_result.has_value(),
                                                "Failed to create TirePressureSkeleton instance!");
    auto service_instance = std::move(service_instance_result).value();

    // A field always has a current value. Therefore, BEFORE the provider may offer the service, it has to supply an
    // initial value for *each* field via Field::Update(). Omitting this for any field would make the OfferService()
    // call fail.
    std::cout << "Setting initial tire pressures (before offering the service):" << std::endl;
    UpdateField(service_instance.tire_pressure_front_left, "tire_pressure_front_left", kInitialTirePressure);
    UpdateField(service_instance.tire_pressure_front_right, "tire_pressure_front_right", kInitialTirePressure);
    UpdateField(service_instance.tire_pressure_rear_left, "tire_pressure_rear_left", kInitialTirePressure);
    UpdateField(service_instance.tire_pressure_rear_right, "tire_pressure_rear_right", kInitialTirePressure);

    auto offer_result = service_instance.OfferService();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(offer_result.has_value(),
                                                "Failed to offer TirePressureSkeleton instance!");
    std::cout << "OfferService: TirePressure service instance is now offered." << std::endl;

    // Give a consumer time to discover, subscribe and observe the *initial* field values before we start updating them.
    std::cout << "Waiting " << kInitialUpdateDelay.count() << " s before updating the fields ..." << std::endl;
    std::this_thread::sleep_for(kInitialUpdateDelay);

    // Random number generators for the randomized update cadence and the randomized field values.
    std::mt19937 random_engine{std::random_device{}()};
    std::uniform_int_distribution<std::chrono::milliseconds::rep> delay_distribution{kMinUpdateDelay.count(),
                                                                                     kMaxUpdateDelay.count()};
    std::uniform_real_distribution<float> pressure_distribution{kMinTirePressure, kMaxTirePressure};

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
        UpdateField(service_instance.tire_pressure_front_left,
                    "tire_pressure_front_left",
                    pressure_distribution(random_engine));
        UpdateField(service_instance.tire_pressure_front_right,
                    "tire_pressure_front_right",
                    pressure_distribution(random_engine));
        UpdateField(
            service_instance.tire_pressure_rear_left, "tire_pressure_rear_left", pressure_distribution(random_engine));
        UpdateField(service_instance.tire_pressure_rear_right,
                    "tire_pressure_rear_right",
                    pressure_distribution(random_engine));
    }
    std::cout << "TirePressure service provider going down." << std::endl;
}
