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
#include "score/mw/com/test/move_semantics/proxy_field/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/move_semantics/proxy_field/test_field_datatype.h"

#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/proxy_field_move_semantics_interprocess_notification"};

/// \brief Registers the "MovedTo" instance's Set-handler transform (DoubleAndIncrement) on the given skeleton.
void RegisterMovedToSetHandler(ProxyFieldMoveSemanticsSkeleton& skeleton)
{
    const auto register_handler_result = skeleton.moved_field_.RegisterSetHandler([](std::int32_t& value) noexcept {
        value = DoubleAndIncrement(value);
    });
    if (!register_handler_result.has_value())
    {
        FailTest("Provider: Unable to register MovedTo set handler: ", register_handler_result.error());
    }
}

/// \brief Registers the "MovedFrom" instance's Set-handler transform (AddOneHundred) on the given skeleton.
void RegisterMovedFromSetHandler(ProxyFieldMoveSemanticsSkeleton& skeleton)
{
    const auto register_handler_result = skeleton.moved_field_.RegisterSetHandler([](std::int32_t& value) noexcept {
        value = AddOneHundred(value);
    });
    if (!register_handler_result.has_value())
    {
        FailTest("Provider: Unable to register MovedFrom set handler: ", register_handler_result.error());
    }
}

void UpdateInitialValue(ProxyFieldMoveSemanticsSkeleton& skeleton, const std::int32_t initial_value)
{
    const auto update_result = skeleton.moved_field_.Update(initial_value);
    if (!update_result.has_value())
    {
        FailTest("Provider: Unable to update field with initial value: ", update_result.error());
    }
}

}  // namespace

void RunProvider(const ProxyMoveScenario& scenario, const score::cpp::stop_token& stop_token)
{
    auto consumer_done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!consumer_done_synchronizer_result.has_value())
    {
        FailTest("proxy_field_move_semantics provider failed: could not create consumer done synchronizer");
    }
    auto& consumer_done_synchronizer = consumer_done_synchronizer_result.value();

    if (scenario == ProxyMoveScenario::kNumberOfScenarios)
    {
        FailTest("proxy_field_move_semantics provider failed: unknown scenario");
    }

    // Step 1. Create skeleton(s). MoveConstruct only needs "MovedTo"; MoveAssign also needs "MovedFrom"
    std::cout << "\nProvider: Step 1 - Create skeleton(s)" << std::endl;
    SkeletonContainer<ProxyFieldMoveSemanticsSkeleton> moved_to_skeleton_container{};
    moved_to_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedTo, "proxy_field_move_semantics");

    // Step 1a. Register set-handler and update initial value for the "MovedTo" instance
    RegisterMovedToSetHandler(moved_to_skeleton_container.GetSkeleton());
    UpdateInitialValue(moved_to_skeleton_container.GetSkeleton(), kInitialValueMovedTo);

    SkeletonContainer<ProxyFieldMoveSemanticsSkeleton> moved_from_skeleton_container{};
    if (scenario == ProxyMoveScenario::kMoveAssignAfterCreate)
    {
        // Create the "MovedFrom" skeleton only for the MoveAssign scenario and register its set-handler and
        //  update its initial value
        moved_from_skeleton_container.CreateSkeleton(kInstanceSpecifierMovedFrom, "proxy_field_move_semantics");
        RegisterMovedFromSetHandler(moved_from_skeleton_container.GetSkeleton());
        UpdateInitialValue(moved_from_skeleton_container.GetSkeleton(), kInitialValueMovedFrom);
    }

    // Step 2. Offer service(s)
    std::cout << "\nProvider: Step 2 - Offer service(s)" << std::endl;
    moved_to_skeleton_container.OfferService("proxy_field_move_semantics");
    if (scenario == ProxyMoveScenario::kMoveAssignAfterCreate)
    {
        moved_from_skeleton_container.OfferService("proxy_field_move_semantics");
    }

    // Step 3. Wait for the consumer to finish its full move -> Subscribe -> GetNewSample/Set/Get -> Unsubscribe ->
    // Subscribe -> GetNewSample sequence.
    std::cout << "\nProvider: Step 3 - Ready, waiting for consumer to finish" << std::endl;
    if (!consumer_done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("proxy_field_move_semantics provider failed: waiting for consumer done was aborted");
    }

    std::cout << "Provider: Shutting down" << std::endl;
}

}  // namespace score::mw::com::test
