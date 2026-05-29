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
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_method_provider.h"

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
const std::string kFailureMessagePrefix{"incomplete_handlers_test"};

constexpr std::int32_t kInArgOnlyMethodTestValueA{17};
constexpr std::int32_t kInArgOnlyMethodTestValueB{18};

void RunIncompleteHandlersTest()
{
    std::cout << "\n=== Incomplete handlers fail OfferService ===" << std::endl;

    std::cout << "\nProvider: Step 1: Create skeleton" << std::endl;
    AllSignaturesMethodProvider provider{};
    provider.CreateSkeleton(kInstanceSpecifier, kFailureMessagePrefix);

    std::cout << "\nProvider: Register only 2 of 4 handlers" << std::endl;
    provider.RegisterMethodHandlerWithInArgsAndReturn(kFailureMessagePrefix);
    provider.RegisterMethodHandlerWithInArgsOnly(
        kInArgOnlyMethodTestValueA, kInArgOnlyMethodTestValueB, kFailureMessagePrefix);

    // Step 3. Try to offer service - should fail
    std::cout << "\nProvider: Try to offer service - should fail" << std::endl;
    const auto was_successfully_offered = provider.GetSkeleton().OfferService();
    if (was_successfully_offered)
    {
        FailTest("ERROR: OfferService succeeded when it should have failed!");
        return;
    }

    std::cout << "=== Incomplete handlers test: PASSED ===" << std::endl;
}

}  // namespace
}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    score::mw::com::test::SetupAssertHandler();
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    score::mw::com::test::RunIncompleteHandlersTest();
    return EXIT_SUCCESS;
}
