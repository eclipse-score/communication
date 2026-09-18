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
#include "score/mw/com/test/move_semantics/skeleton_field/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/move_semantics/skeleton_field/test_field_datatype.h"
#include "score/mw/com/test/move_semantics/skeleton_field/test_parameters.h"
#include "score/mw/com/types.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/skeleton_field_move_semantics_interprocess_notification"};
const std::string kAboutToCallShmPath{"/skeleton_field_move_semantics_about_to_call"};

template <typename ProxyFieldType>
void CallGetAndCheckValue(ProxyFieldType& proxy_field, const std::int32_t expected_value)
{
    const auto get_result = proxy_field.Get();
    if (!get_result.has_value())
    {
        FailTest("Consumer: Get() failed: ", get_result.error());
    }
    if (*(get_result.value()) != expected_value)
    {
        FailTest("Consumer: Get() returned ", *(get_result.value()), " but expected ", expected_value);
    }
    std::cout << "\nConsumer: Get() returned expected value " << expected_value << std::endl;
}

template <typename ProxyFieldType>
void CallSetAndCheckReturnValue(ProxyFieldType& proxy_field,
                                const std::int32_t set_request_value,
                                const std::int32_t expected_accepted_value)
{
    const auto set_result = proxy_field.Set(set_request_value);
    if (!set_result.has_value())
    {
        FailTest("Consumer: Set() failed: ", set_result.error());
    }
    const std::int32_t accepted_value = *(set_result.value());
    if (accepted_value != expected_accepted_value)
    {
        FailTest("Consumer: Set() returned accepted value ", accepted_value, " but expected ", expected_accepted_value);
    }
    std::cout << "\nConsumer: Set() returned expected accepted value " << accepted_value << std::endl;
}

}  // namespace

void RunConsumer(const score::cpp::stop_token& stop_token)
{
    auto done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create done ProcessSynchronizer");
    }
    auto about_to_call_synchronizer_result = ProcessSynchronizer::Create(kAboutToCallShmPath);
    if (!about_to_call_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create about-to-call ProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&done_synchronizer_result]() {
        done_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<SkeletonFieldMoveSemanticsProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "skeleton_field_move_semantics");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Register receive handler and state change handler
    std::cout << "\nConsumer: Step 2 - Register receive and state change handlers" << std::endl;
    ProxyEventReceiver field_receiver{proxy.moved_field_};
    ProxyEventStateChangeNotifier subscription_notifier{proxy.moved_field_};

    // Step 3. Subscribe
    std::cout << "\nConsumer: Step 3 - Subscribe" << std::endl;
    const auto subscribe_result = proxy.moved_field_.Subscribe(kTotalNumValuesToSend);
    if (!subscribe_result.has_value())
    {
        FailTest("Consumer: Subscribe failed: ", subscribe_result.error());
    }

    // Step 4. Wait for subscription
    std::cout << "\nConsumer: Step 4 - Wait for subscription" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed");
    }

    // Step 5. Notify provider that the live API call sequence is about to start. For "before offer" scenarios, the
    // provider never waits on this notification, so this call is a harmless no-op there.
    std::cout << "\nConsumer: Step 5 - Notify provider about to call" << std::endl;
    about_to_call_synchronizer_result->Notify();

    // Step 6. Live sequence: WaitForSamples -> Set -> Get. For "after offer" scenarios, the provider's move races
    // this whole sequence with a single random delay, so it may land inside any of the 3 calls.
    std::cout << "\nConsumer: Step 6 - Wait for all expected samples" << std::endl;
    const std::vector<std::int32_t> values_to_receive = {kInitialValue, 20, 30, 35};
    if (!field_receiver.WaitForSamples(stop_token, values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples");
    }

    std::cout << "\nConsumer: Step 6 - Set field value and verify accepted value" << std::endl;
    const auto expected_accepted_set_value = (kSetRequestValue * 2) + 1;
    CallSetAndCheckReturnValue(proxy.moved_field_, kSetRequestValue, expected_accepted_set_value);

    std::cout << "\nConsumer: Step 6 - Get field value and verify it matches the accepted Set() value" << std::endl;
    CallGetAndCheckValue(proxy.moved_field_, expected_accepted_set_value);

    // Step 7. Unsubscribe
    std::cout << "\nConsumer: Step 7 - Unsubscribe" << std::endl;
    proxy.moved_field_.Unsubscribe();
}

}  // namespace score::mw::com::test
