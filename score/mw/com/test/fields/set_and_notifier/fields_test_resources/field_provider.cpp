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

#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/field_provider.h"

#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/test_datatype.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace score::mw::com::test
{
namespace
{

int run_notifier_service(const std::chrono::milliseconds& cycle_time, const score::cpp::stop_token& stop_token)
{
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Unable to create instance specifier, terminating\n";
        return -3;
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    auto service_result = InitialOnlySkeleton::Create(std::move(instance_specifier));
    if (!service_result.has_value())
    {
        std::cerr << "Unable to construct InitialOnlySkeleton: " << service_result.error() << ", bailing!\n";
        return -4;
    }

    InitialOnlySkeleton& service{service_result.value()};

    const auto update_result = service.test_field.Update(kInitialValue);
    if (!update_result.has_value())
    {
        std::cerr << "Unable to update initial field value: " << update_result.error() << ", bailing!\n";
        return -6;
    }
    const auto offer_result = service.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer InitialOnlySkeleton: " << offer_result.error() << ", bailing!\n";
        return -5;
    }

    while (!stop_token.stop_requested())
    {
        std::this_thread::sleep_for(cycle_time);
    }

    service.StopOfferService();

    return 0;
}

int run_set_service(const std::chrono::milliseconds& cycle_time, const score::cpp::stop_token& stop_token)
{
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Unable to create instance specifier, terminating\n";
        return -13;
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    auto service_result = SetEnabledSkeleton::Create(std::move(instance_specifier));
    if (!service_result.has_value())
    {
        std::cerr << "Unable to construct SetEnabledSkeleton: " << service_result.error() << ", bailing!\n";
        return -14;
    }

    SetEnabledSkeleton& service{service_result.value()};
    const auto register_handler_result = service.test_field.RegisterSetHandler([](std::int32_t& value) noexcept {
        if (value > kSetAcceptedValue)
        {
            value = kSetAcceptedValue;
        }
        if (value < 0)
        {
            value = 0;
        }
    });
    if (!register_handler_result.has_value())
    {
        std::cerr << "Unable to register set handler: " << register_handler_result.error() << ", bailing!\n";
        return -15;
    }

    const auto update_result = service.test_field.Update(kInitialValue);
    if (!update_result.has_value())
    {
        std::cerr << "Unable to update initial field value: " << update_result.error() << ", bailing!\n";
        return -16;
    }

    const auto offer_result = service.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer SetEnabledSkeleton: " << offer_result.error() << ", bailing!\n";
        return -17;
    }

    while (!stop_token.stop_requested())
    {
        std::this_thread::sleep_for(cycle_time);
    }

    service.StopOfferService();

    return 0;
}

}  // namespace

int run_service(const std::chrono::milliseconds& cycle_time,
                const score::cpp::stop_token& stop_token,
                const std::string& mode)
{
    if (mode == "notifier")
    {
        return run_notifier_service(cycle_time, stop_token);
    }
    if (mode == "set")
    {
        return run_set_service(cycle_time, stop_token);
    }

    // TODO: Add "get" mode service scenario coverage once getter-enabled field variant is introduced.
    std::cerr << "Unsupported mode passed to service: " << mode << "\n";
    return -1;
}

}  // namespace score::mw::com::test
