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
#include "score/mw/com/com_error_domain.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_method_provider.h"
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace score::mw::com::test
{
namespace
{

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/edge_cases_test/MethodSignature"}).value();
const std::string kFailureMessagePrefix{"disabled_method_test"};

constexpr std::int32_t kReturnOnlyMethodReturnValue{15};
constexpr std::int32_t kInArgOnlyMethodTestValueA{17};
constexpr std::int32_t kInArgOnlyMethodTestValueB{18};

constexpr std::int32_t kTestValueA{10};
constexpr std::int32_t kTestValueB{20};

template <typename ResultType>
void FailOnUnexpectedSuccess(const std::string& method_name, const ResultType& call_result, ComErrc expected_error_code)
{
    if (call_result.has_value() || call_result.error() != expected_error_code)
    {
        FailTest(kFailureMessagePrefix, " Consumer: ", method_name, " unexpectedly succeeded.");
    }

    std::cout << "Consumer: " << method_name << " failed as expected: " << call_result.error() << std::endl;
}

void RunDisabledMethodTest()
{
    std::cout << "\n=== Disabled methods return errors ===" << std::endl;

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
    ProxyContainer<AllSignaturesProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, kFailureMessagePrefix);
    auto& proxy = proxy_container.GetProxy();

    std::cout << "\nConsumer: Step 5: Call methods which have been disabled in the C++ interface" << std::endl;
    const auto expected_error_code = ComErrc::kMethodBindingDisabled;
    FailOnUnexpectedSuccess(
        "with_in_args_and_return", proxy.with_in_args_and_return(kTestValueA, kTestValueB), expected_error_code);
    FailOnUnexpectedSuccess(
        "with_in_args_only", proxy.with_in_args_only(kTestValueA, kTestValueB), expected_error_code);
    FailOnUnexpectedSuccess("with_return_only", proxy.with_return_only(), expected_error_code);
    FailOnUnexpectedSuccess("without_args_or_return", proxy.without_args_or_return(), expected_error_code);

    std::cout << "=== Disabled method test: PASSED ===" << std::endl;
}

}  // namespace
}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    const auto service_instance_manifest = score::mw::com::test::ParseServiceInstanceManifest(argc, argv);

    score::mw::com::test::SetupAssertHandler();
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest});

    score::mw::com::test::RunDisabledMethodTest();
    return EXIT_SUCCESS;
}
