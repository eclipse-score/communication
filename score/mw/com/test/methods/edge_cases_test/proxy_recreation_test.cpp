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
const std::string kFailureMessagePrefix{"proxy_recreation_test"};

constexpr std::int32_t kReturnOnlyMethodReturnValue{15};
constexpr std::int32_t kInArgOnlyMethodTestValueA{17};
constexpr std::int32_t kInArgOnlyMethodTestValueB{18};

constexpr std::int32_t kTestValueA{10};
constexpr std::int32_t kTestValueB{20};

void RunProxyRecreationTest()
{
    std::cout << "\n=== Proxy recreation ===" << std::endl;

    AllSignaturesMethodProvider provider{};

    std::cout << "\nProvider: Step 1: Create skeleton" << std::endl;
    provider.CreateSkeleton(kInstanceSpecifier, kFailureMessagePrefix);

    std::cout << "\nProvider: Step 2: Register method handlers" << std::endl;
    provider.RegisterMethodHandlerWithInArgsAndReturn(kFailureMessagePrefix);
    provider.RegisterMethodHandlerWithInArgsOnly(
        kInArgOnlyMethodTestValueA, kInArgOnlyMethodTestValueB, kFailureMessagePrefix);
    provider.RegisterMethodHandlerWithReturnOnly(kReturnOnlyMethodReturnValue, kFailureMessagePrefix);
    provider.RegisterWithoutInArgsOrReturn(kFailureMessagePrefix);

    std::cout << "\nProvider: Step 3: Offer Service" << std::endl;
    provider.OfferService(kFailureMessagePrefix);

    {
        std::cout << "\nConsumer: Step 4: Find service and create proxy" << std::endl;
        AllSignaturesMethodConsumer consumer{};
        consumer.CreateProxy(kInstanceSpecifier, kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 5: Call method" << std::endl;
        consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, AllSignaturesMethodConsumer::CopyMode::WITH_COPY, kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 6: Destroy proxy" << std::endl;
    }

    {
        std::cout << "\nConsumer: Step 7: Find service and create second proxy" << std::endl;
        AllSignaturesMethodConsumer consumer{};
        consumer.CreateProxy(kInstanceSpecifier, kFailureMessagePrefix);

        std::cout << "\nConsumer: Step 8: Call method" << std::endl;
        consumer.CallMethodWithInArgsAndReturn(
            kTestValueA, kTestValueB, AllSignaturesMethodConsumer::CopyMode::WITH_COPY, kFailureMessagePrefix);
    }
    std::cout << "=== Proxy recreation test: PASSED ===" << std::endl;
}

}  // namespace
}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    auto service_instance_manifest_path = score::mw::com::test::ParseServiceInstanceManifest(argc, argv);

    score::mw::com::test::SetupAssertHandler();
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest_path});

    score::mw::com::test::RunProxyRecreationTest();
    return EXIT_SUCCESS;
}
