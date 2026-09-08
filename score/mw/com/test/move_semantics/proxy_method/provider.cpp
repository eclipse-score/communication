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
#include "score/mw/com/test/move_semantics/proxy_method/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/move_semantics/proxy_method/test_method_datatype.h"

#include <cstdint>
#include <iostream>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/proxy_method_move_semantics_interprocess_notification"};

/// \brief Registers the addition handler (return_value = a + b) on the given skeleton.
void RegisterAdditionHandler(ProxyMethodMoveSemanticsSkeleton& skeleton)
{
    auto handler = [](std::int32_t& return_value, const std::int32_t& a, const std::int32_t& b) {
        std::cout << "Provider: addition handler called with " << a << " + " << b << std::endl;
        return_value = a + b;
    };
    const auto register_result = skeleton.with_in_args_and_return.RegisterHandler(std::move(handler));
    if (!register_result.has_value())
    {
        FailTest("Provider: Failed to register addition handler: ", register_result.error());
    }
}

/// \brief Registers the subtraction handler (return_value = a - b) on the given skeleton.
void RegisterSubtractionHandler(ProxyMethodMoveSemanticsSkeleton& skeleton)
{
    auto handler = [](std::int32_t& return_value, const std::int32_t& a, const std::int32_t& b) {
        std::cout << "Provider: subtraction handler called with " << a << " - " << b << std::endl;
        return_value = a - b;
    };
    const auto register_result = skeleton.with_in_args_and_return.RegisterHandler(std::move(handler));
    if (!register_result.has_value())
    {
        FailTest("Provider: Failed to register subtraction handler: ", register_result.error());
    }
}

void RunProviderMoveConstruct(const score::cpp::stop_token& stop_token, ProcessSynchronizer& consumer_done_synchronizer)
{
    // Step 1. Create skeleton for the moved-to instance
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<ProxyMethodMoveSemanticsSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "proxy_method_move_semantics");
    auto& skeleton = skeleton_container.GetSkeleton();

    // Step 2. Register the addition handler
    std::cout << "\nProvider: Step 2 - Register method handler" << std::endl;
    RegisterAdditionHandler(skeleton);

    // Step 3. Offer service
    std::cout << "\nProvider: Step 3 - Offer service" << std::endl;
    skeleton_container.OfferService("proxy_method_move_semantics");

    // Step 4. Wait for the consumer to finish all method calls
    std::cout << "\nProvider: Step 4 - Ready for method calls, waiting for consumer to finish" << std::endl;
    if (!consumer_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("proxy_method_move_semantics provider failed: waiting for consumer done was aborted");
    }

    std::cout << "Provider: Shutting down" << std::endl;
}

void RunProviderMoveAssign(const score::cpp::stop_token& stop_token, ProcessSynchronizer& consumer_done_synchronizer)
{
    // Step 1. Create two skeletons. The moved-from instance answers with a + b, the moved-to instance with a - b, so
    // that the consumer can prove that a moved-to proxy reaches the moved-from method channel after a move assignment.
    std::cout << "\nProvider: Step 1 - Create two skeletons" << std::endl;
    SkeletonContainer<ProxyMethodMoveSemanticsSkeleton> moved_from_skeleton_container{};
    moved_from_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "proxy_method_move_semantics");
    RegisterAdditionHandler(moved_from_skeleton_container.GetSkeleton());

    SkeletonContainer<ProxyMethodMoveSemanticsSkeleton> moved_to_skeleton_container{};
    moved_to_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "proxy_method_move_semantics");
    RegisterSubtractionHandler(moved_to_skeleton_container.GetSkeleton());

    // Step 2. Offer both services
    std::cout << "\nProvider: Step 2 - Offer both services" << std::endl;
    moved_from_skeleton_container.OfferService("proxy_method_move_semantics");
    moved_to_skeleton_container.OfferService("proxy_method_move_semantics");

    // Step 3. Wait for the consumer to finish all method calls
    std::cout << "\nProvider: Step 3 - Ready for method calls, waiting for consumer to finish" << std::endl;
    if (!consumer_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("proxy_method_move_semantics provider failed: waiting for consumer done was aborted");
    }

    std::cout << "Provider: Shutting down" << std::endl;
}

}  // namespace

void RunProvider(const ProxyMoveScenario& scenario, const score::cpp::stop_token& stop_token)
{
    auto consumer_done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!consumer_done_synchronizer_result.has_value())
    {
        FailTest("proxy_method_move_semantics provider failed: could not create consumer done synchronizer");
    }

    switch (scenario)
    {
        case ProxyMoveScenario::kMoveConstructAfterMethodCall:
        {
            RunProviderMoveConstruct(stop_token, consumer_done_synchronizer_result.value());
            break;
        }
        case ProxyMoveScenario::kMoveAssignAfterMethodCall:
        {
            RunProviderMoveAssign(stop_token, consumer_done_synchronizer_result.value());
            break;
        }
        case ProxyMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
            FailTest("proxy_method_move_semantics provider failed: unknown scenario");
    }
}

}  // namespace score::mw::com::test
