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

#include "score/mw/com/test/concurrent_skeleton_creation/concurrent_skeleton_creation_application.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/big_datatype.h"
#include "score/mw/com/types.h"

#include <amp_jthread.hpp>
#include <amp_stop_token.hpp>

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

void CreateAndOfferSkeleton(const bmw::mw::com::InstanceSpecifier &instance_specifier, std::atomic_bool &success_flag)
{
    for (std::size_t j = 0; j < 10; ++j)
    {
        auto bigdata_result = bmw::mw::com::test::BigDataSkeleton::Create(instance_specifier);
        if (!bigdata_result.has_value())
        {
            success_flag = false;
            std::cerr << "Could not create skeleton with instance specifier"
                      << instance_specifier.ToString().to_string() << "in index " << j << "of loop, terminating."
                      << std::endl;
            break;
        }
        const auto offer_service_result = bigdata_result->OfferService();
        if (!offer_service_result.has_value())
        {
            success_flag = false;
            std::cerr << "Could not offer service for skeleton with instance specifier"
                      << instance_specifier.ToString().to_string() << "in index " << j << "of loop, terminating."
                      << std::endl;
            break;
        }
    }
}

/**
 * Test that checks that skeletons with different instance IDs of the same service type can be created and offered at
 * the same time.
 */
int main(int argc, const char **argv)
{
    bmw::mw::com::runtime::InitializeRuntime(argc, argv);

    const auto instance_specifier_result_1 = bmw::mw::com::InstanceSpecifier::Create("xpad/cp60/MapApiLanesStamped1");
    if (!instance_specifier_result_1.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;
        return EXIT_FAILURE;
    }
    const auto instance_specifier_result_2 = bmw::mw::com::InstanceSpecifier::Create("xpad/cp60/MapApiLanesStamped2");
    if (!instance_specifier_result_2.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;
        return EXIT_FAILURE;
    }
    const auto instance_specifier_result_3 = bmw::mw::com::InstanceSpecifier::Create("xpad/cp60/MapApiLanesStamped3");
    if (!instance_specifier_result_3.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;
        return EXIT_FAILURE;
    }
    auto &instance_specifier_1 = instance_specifier_result_1.value();
    auto &instance_specifier_2 = instance_specifier_result_2.value();
    auto &instance_specifier_3 = instance_specifier_result_3.value();

    std::atomic_bool success_flag{true};
    amp::jthread skeletion_creation_and_offer_instance_1{
        [&success_flag, &instance_specifier_1]()
        { CreateAndOfferSkeleton(instance_specifier_1, success_flag); }};
    amp::jthread skeletion_creation_and_offer_instance_2{
        [&success_flag, &instance_specifier_2]()
        { CreateAndOfferSkeleton(instance_specifier_2, success_flag); }};
    amp::jthread skeletion_creation_and_offer_instance_3{
        [&success_flag, &instance_specifier_3]()
        { CreateAndOfferSkeleton(instance_specifier_3, success_flag); }};

    auto join_thread = [](amp::jthread &thread)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    };
    join_thread(skeletion_creation_and_offer_instance_1);
    join_thread(skeletion_creation_and_offer_instance_2);
    join_thread(skeletion_creation_and_offer_instance_3);

    return success_flag ? EXIT_SUCCESS : EXIT_FAILURE;
}
