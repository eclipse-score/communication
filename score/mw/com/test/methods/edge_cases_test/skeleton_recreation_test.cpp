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
#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_method_consumer.h"
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_method_provider.h"
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"

#include "score/mw/com/runtime.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/edge_cases_test/MethodSignature"}).value();
const std::string kFailureMessagePrefix{"skeleton_recreation_test"};

constexpr std::int32_t kReturnOnlyMethodReturnValue{15};
constexpr std::int32_t kInArgOnlyMethodTestValueA{17};
constexpr std::int32_t kInArgOnlyMethodTestValueB{18};

constexpr std::int32_t kTestValueA{10};
constexpr std::int32_t kTestValueB{20};

void RunSkeletonRecreationWithNewProxyTest()
{
    std::cout << "\n=== Skeleton recreation with new proxy ===" << std::endl;

    {
        std::cout << "\nProvider: Step 1: Create skeleton" << std::endl;
        AllSignaturesMethodProvider provider{};
        provider.CreateSkeleton(kInstanceSpecifier, kFailureMessagePrefix);

        std::cout << "\nProvider: Step 2: Register method handlers" << std::endl;
        provider.RegisterMethodHandlerWithInArgsAndReturn(kFailureMessagePrefix);
        provider.RegisterMethodHandlerWithInArgsOnly(
            kInArgOnlyMethodTestValueA, kInArgOnlyMethodTestValueB, kFailureMessagePrefix);
        provider.RegisterMethodHandlerWithReturnOnly(kReturnOnlyMethodReturnValue, kFailureMessagePrefix);
        provider.RegisterWithoutInArgsOrReturn(kFailureMessagePrefix);

        std::cout << "\nProvider: Step 3: Offer Service" << std::endl;
        provider.OfferService(kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 4: Find service and create proxy" << std::endl;
        AllSignaturesMethodConsumer consumer{};
        consumer.CreateProxy(kInstanceSpecifier, kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 5: Call method" << std::endl;
        consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, AllSignaturesMethodConsumer::CopyMode::WITH_COPY, kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 6: Destroy proxy and skeleton" << std::endl;
    }

    {
        std::cout << "\nProvider: Step 7: Create second skeleton" << std::endl;
        AllSignaturesMethodProvider provider{};
        provider.CreateSkeleton(kInstanceSpecifier, kFailureMessagePrefix);

        std::cout << "\nProvider: Step 8: Register method handlers" << std::endl;
        provider.RegisterMethodHandlerWithInArgsAndReturn(kFailureMessagePrefix);
        provider.RegisterMethodHandlerWithInArgsOnly(
            kInArgOnlyMethodTestValueA, kInArgOnlyMethodTestValueB, kFailureMessagePrefix);
        provider.RegisterMethodHandlerWithReturnOnly(kReturnOnlyMethodReturnValue, kFailureMessagePrefix);
        provider.RegisterWithoutInArgsOrReturn(kFailureMessagePrefix);

        std::cout << "\nProvider: Step 9: Offer second service" << std::endl;
        provider.OfferService(kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 10: Find service and create second proxy" << std::endl;
        AllSignaturesMethodConsumer consumer{};
        consumer.CreateProxy(kInstanceSpecifier, kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 11: Call the same method again" << std::endl;
        consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, AllSignaturesMethodConsumer::CopyMode::ZERO_COPY, kFailureMessagePrefix);
    }
}

void RunSkeletonRecreationWithSameProxyTest(const score::cpp::stop_token& stop_token)
{
    std::cout << "\n=== Skeleton recreation with same proxy ===" << std::endl;

    AllSignaturesMethodConsumer consumer{};
    {
        std::cout << "\nProvider: Step 1: Create skeleton" << std::endl;
        AllSignaturesMethodProvider provider{};
        provider.CreateSkeleton(kInstanceSpecifier, kFailureMessagePrefix);

        std::cout << "\nProvider: Step 2: Register method handlers" << std::endl;
        provider.RegisterMethodHandlerWithInArgsAndReturn(kFailureMessagePrefix);
        provider.RegisterMethodHandlerWithInArgsOnly(
            kInArgOnlyMethodTestValueA, kInArgOnlyMethodTestValueB, kFailureMessagePrefix);
        provider.RegisterMethodHandlerWithReturnOnly(kReturnOnlyMethodReturnValue, kFailureMessagePrefix);
        provider.RegisterWithoutInArgsOrReturn(kFailureMessagePrefix);

        std::cout << "\nProvider: Step 3: Offer Service" << std::endl;
        provider.OfferService(kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 4: Find service and create proxy" << std::endl;
        consumer.CreateProxy(kInstanceSpecifier, kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 5: Call method" << std::endl;
        consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, AllSignaturesMethodConsumer::CopyMode::WITH_COPY, kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 6: Destroy skeleton only" << std::endl;
    }

    {
        std::cout << "\nProvider: Step 7: Create second skeleton" << std::endl;
        AllSignaturesMethodProvider provider{};
        provider.CreateSkeleton(kInstanceSpecifier, kFailureMessagePrefix);

        std::cout << "\nProvider: Step 8: Register method handlers" << std::endl;
        provider.RegisterMethodHandlerWithInArgsAndReturn(kFailureMessagePrefix);
        provider.RegisterMethodHandlerWithInArgsOnly(
            kInArgOnlyMethodTestValueA, kInArgOnlyMethodTestValueB, kFailureMessagePrefix);
        provider.RegisterMethodHandlerWithReturnOnly(kReturnOnlyMethodReturnValue, kFailureMessagePrefix);
        provider.RegisterWithoutInArgsOrReturn(kFailureMessagePrefix);

        std::cout << "\nProvider: Step 9: Offer second service" << std::endl;
        provider.OfferService(kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 10: Call method again on old proxy" << std::endl;

        std::cout << "\nConsumer: Step 10.1: Wait until proxy has reconnected to recreated skeleton" << std::endl;
        // Since we have no way of being notified of the proxy being reconnected to the new skeleton, we keep calling a
        // method in a loop until it succeeds. If it succeeded then it indicates that the proxy reconnected.
        while (!stop_token.stop_requested() && !consumer.GetProxy().without_args_or_return().has_value())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        if (stop_token.stop_requested())
        {
            FailTest(kFailureMessagePrefix, " Consumer: Stop token was requested before proxy reconnected to skeleton");
        }

        std::cout << "\nConsumer: Step 10.2: Call the same method again" << std::endl;
        consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, AllSignaturesMethodConsumer::CopyMode::ZERO_COPY, kFailureMessagePrefix);
    }
}

}  // namespace
}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    auto service_instance_manifest_path = score::mw::com::test::ParseServiceInstanceManifest(argc, argv);

    score::mw::com::test::SetupAssertHandler();
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest_path});

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    score::mw::com::test::RunSkeletonRecreationWithNewProxyTest();
    score::mw::com::test::RunSkeletonRecreationWithSameProxyTest(stop_source.get_token());

    return EXIT_SUCCESS;
}
