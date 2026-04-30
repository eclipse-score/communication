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
#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/methods/edge_cases_test/test_method_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/proxy_container.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace score::mw::com::test
{
namespace
{

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/edge_cases/EdgeCasesMethods"}).value();

constexpr std::int32_t kTestValueA = 10;
constexpr std::int32_t kTestValueB = 20;
constexpr std::int32_t kExpectedSum = kTestValueA + kTestValueB;

const std::vector<std::string_view> kAllMethodNames = {"method_with_args_and_return",
                                                       "method_with_args_only",
                                                       "method_with_return_only",
                                                       "method_without_args_or_return"};

// Test 1: Disabled methods should return error (not crash), enabled methods should work
//
// Creates a proxy with 3 of 4 methods enabled and verifies:
//   - All 3 enabled methods (args+return, args-only, return-only) work correctly
//   - The disabled void() method returns an error instead of crashing
//
// NOTE: Framework limitation — only the void() signature gracefully returns an error when
// the method is disabled. For other signatures the impl layer (proxy_method.h) wraps binding
// calls with SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD assertions that abort the process:
//   - void(Args...):       AllocateInArgs → SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD → abort
//   - ReturnType():        AllocateReturnType → SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD → abort
//   - ReturnType(Args...): AllocateInArgs → SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD → abort
// The LoLa binding correctly returns kBindingFailure for unsubscribed methods, but the error
// is never propagated. When the impl layer is fixed to propagate errors instead of asserting,
// extend this test to cover all four signatures as disabled.
void TestDisabledMethodReturnsError()
{
    std::cout << "\n=== Test 1: Disabled method returns error, enabled methods work ===" << std::endl;

    // Create skeleton and register all handlers
    auto skeleton_result = EdgeCasesMethodSkeleton::Create(kInstanceSpecifier);
    if (!skeleton_result.has_value())
    {
        FailTest("Failed to create skeleton: ", skeleton_result.error());
    }
    auto skeleton = std::make_unique<EdgeCasesMethodSkeleton>(std::move(skeleton_result).value());

    auto handler1 = [](std::int32_t val1, std::int32_t val2) -> std::int32_t {
        return val1 + val2;
    };
    skeleton->method_with_args_and_return.RegisterHandler(std::move(handler1));

    auto handler2 = [](std::int32_t) {};
    skeleton->method_with_args_only.RegisterHandler(std::move(handler2));

    auto handler3 = []() -> std::int32_t {
        return 42;
    };
    skeleton->method_with_return_only.RegisterHandler(std::move(handler3));

    auto handler4 = []() {};
    skeleton->method_without_args_or_return.RegisterHandler(std::move(handler4));

    auto offer_result = skeleton->OfferService();
    if (!offer_result.has_value())
    {
        FailTest("Failed to offer service: ", offer_result.error());
    }

    // Create proxy with 3 methods enabled, leaving method_without_args_or_return DISABLED
    ProxyContainer<EdgeCasesMethodProxy> proxy_container;
    const std::vector<std::string_view> enabled_methods = {
        "method_with_args_and_return", "method_with_args_only", "method_with_return_only"};
    if (!proxy_container.CreateProxy(kInstanceSpecifier, enabled_methods))
    {
        FailTest("Failed to create proxy");
    }

    // 1a: Verify enabled method_with_args_and_return works (int32_t(int32_t, int32_t))
    std::cout << "  Testing enabled method_with_args_and_return..." << std::endl;
    auto result_args_return = proxy_container.GetProxy().method_with_args_and_return(kTestValueA, kTestValueB);
    if (!result_args_return.has_value())
    {
        FailTest("Enabled method_with_args_and_return failed: ", result_args_return.error());
    }
    if (*result_args_return.value() != kExpectedSum)
    {
        FailTest(
            "method_with_args_and_return wrong value: ", *result_args_return.value(), " expected: ", kExpectedSum);
    }
    std::cout << "  PASSED - method_with_args_and_return works" << std::endl;

    // 1b: Verify enabled method_with_args_only works (void(int32_t))
    std::cout << "  Testing enabled method_with_args_only..." << std::endl;
    auto result_args_only = proxy_container.GetProxy().method_with_args_only(kTestValueA);
    if (!result_args_only.has_value())
    {
        FailTest("Enabled method_with_args_only failed: ", result_args_only.error());
    }
    std::cout << "  PASSED - method_with_args_only works" << std::endl;

    // 1c: Verify enabled method_with_return_only works (int32_t())
    std::cout << "  Testing enabled method_with_return_only..." << std::endl;
    auto result_return_only = proxy_container.GetProxy().method_with_return_only();
    if (!result_return_only.has_value())
    {
        FailTest("Enabled method_with_return_only failed: ", result_return_only.error());
    }
    constexpr std::int32_t kExpectedReturnOnly = 42;
    if (*result_return_only.value() != kExpectedReturnOnly)
    {
        FailTest(
            "method_with_return_only wrong value: ", *result_return_only.value(), " expected: ", kExpectedReturnOnly);
    }
    std::cout << "  PASSED - method_with_return_only works" << std::endl;

    // 1d: Call DISABLED method_without_args_or_return → should return error (not crash)
    // This is the void() signature — the only one that currently returns an error gracefully.
    std::cout << "  Testing disabled method_without_args_or_return..." << std::endl;
    auto disabled_result = proxy_container.GetProxy().method_without_args_or_return();
    if (disabled_result.has_value())
    {
        FailTest("ERROR: Disabled method succeeded when it should have returned error!", disabled_result.error());
    }
    std::cout << "  PASSED - Disabled method returned error: " << disabled_result.error() << std::endl;

    skeleton->StopOfferService();

    std::cout << "=== Test 1: PASSED ===" << std::endl;
}

// Test 2: Skeleton without all handlers should fail to offer service
void TestIncompleteHandlers()
{
    std::cout << "\n=== Test 2: Incomplete handlers fail OfferService ===" << std::endl;

    auto skeleton_result = EdgeCasesMethodSkeleton::Create(kInstanceSpecifier);
    if (!skeleton_result.has_value())
    {
        FailTest("Failed to create skeleton: ", skeleton_result.error());
    }
    auto skeleton = std::make_unique<EdgeCasesMethodSkeleton>(std::move(skeleton_result).value());

    // Register only 2 of 4 handlers
    auto handler1 = [](std::int32_t val1, std::int32_t val2) -> std::int32_t {
        return val1 + val2;
    };
    skeleton->method_with_args_and_return.RegisterHandler(std::move(handler1));
    std::cout << "  Registered handler 1" << std::endl;

    auto handler2 = [](std::int32_t) {};
    skeleton->method_with_args_only.RegisterHandler(std::move(handler2));
    std::cout << "  Registered handler 2 (only 2 of 4 registered)" << std::endl;

    // Try to offer service - should fail
    auto offer_result = skeleton->OfferService();
    if (offer_result.has_value())
    {
        FailTest("ERROR: OfferService succeeded when it should have failed!");
        skeleton->StopOfferService();
    }

    std::cout << "  OfferService failed as expected: " << offer_result.error() << std::endl;
    std::cout << "=== Test 2: PASSED ===" << std::endl;
}

// Test 3: Proxy can be recreated after first one is destroyed
void TestProxyRecreation()
{
    std::cout << "\n=== Test 3: Proxy recreation ===" << std::endl;

    // Create skeleton with all handlers
    auto skeleton_result = EdgeCasesMethodSkeleton::Create(kInstanceSpecifier);
    if (!skeleton_result.has_value())
    {
        FailTest("Failed to create skeleton: ", skeleton_result.error());
    }
    auto skeleton = std::make_unique<EdgeCasesMethodSkeleton>(std::move(skeleton_result).value());

    auto handler1 = [](std::int32_t val1, std::int32_t val2) -> std::int32_t {
        return val1 + val2;
    };
    skeleton->method_with_args_and_return.RegisterHandler(std::move(handler1));

    auto handler2 = [](std::int32_t) {};
    skeleton->method_with_args_only.RegisterHandler(std::move(handler2));

    auto handler3 = []() -> std::int32_t {
        return 42;
    };
    skeleton->method_with_return_only.RegisterHandler(std::move(handler3));

    auto handler4 = []() {};
    skeleton->method_without_args_or_return.RegisterHandler(std::move(handler4));

    auto offer_result = skeleton->OfferService();
    if (!offer_result.has_value())
    {
        FailTest("Failed to offer service: ", offer_result.error());
    }

    // Create first proxy, use it, then destroy it
    {
        std::cout << "  Creating first proxy..." << std::endl;
        ProxyContainer<EdgeCasesMethodProxy> proxy_container1;
        FailTestIf(!proxy_container1.CreateProxy(kInstanceSpecifier, kAllMethodNames),
                     "Failed to create first proxy");

        auto result1 = proxy_container1.GetProxy().method_with_args_and_return(kTestValueA, kTestValueB);
        if (!result1.has_value())
        {
            FailTest("First proxy method call failed: ", result1.error());
        }
        FailTestIf(*result1.value() != kExpectedSum, "First proxy returned wrong value");

        std::cout << "  First proxy works correctly" << std::endl;
    }  // First proxy destroyed here

    // Create second proxy after first is destroyed
    {
        std::cout << "  Creating second proxy after first is destroyed..." << std::endl;
        ProxyContainer<EdgeCasesMethodProxy> proxy_container2;
        FailTestIf(!proxy_container2.CreateProxy(kInstanceSpecifier, kAllMethodNames),
                     "Failed to create second proxy");

        auto result2 = proxy_container2.GetProxy().method_with_args_and_return(kTestValueA, kTestValueB);
        if (!result2.has_value())
        {
            FailTest("Second proxy method call failed: ", result2.error());
        }
        FailTestIf(*result2.value() != kExpectedSum, "Second proxy returned wrong value");

        std::cout << "  Second proxy works correctly" << std::endl;
    }

    skeleton->StopOfferService();

    std::cout << "=== Test 3: PASSED ===" << std::endl;
}

// Test 4: Skeleton can be recreated after first one is destroyed
//
// Currently blocked by Ticket-243577: StopOfferService does not unregister the
// OnServiceMethodSubscribedHandler, so the leaked handler causes the second OfferService
// to fail with kBindingFailure. Enable this test by setting RUN_SKELETON_RECREATION_TEST=1
// once the fix lands.
void TestSkeletonRecreation()
{
    std::cout << "\n=== Test 4: Skeleton recreation ===" << std::endl;

    // --- First skeleton lifecycle ---
    {
        std::cout << "  Creating first skeleton..." << std::endl;
        auto skeleton_result = EdgeCasesMethodSkeleton::Create(kInstanceSpecifier);
        if (!skeleton_result.has_value())
        {
            FailTest("Failed to create first skeleton: ", skeleton_result.error());
        }
        auto skeleton = std::make_unique<EdgeCasesMethodSkeleton>(std::move(skeleton_result).value());

        auto handler1 = [](std::int32_t val1, std::int32_t val2) -> std::int32_t {
            return val1 + val2;
        };
        skeleton->method_with_args_and_return.RegisterHandler(std::move(handler1));

        auto handler2 = [](std::int32_t) {};
        skeleton->method_with_args_only.RegisterHandler(std::move(handler2));

        auto handler3 = []() -> std::int32_t {
            return 42;
        };
        skeleton->method_with_return_only.RegisterHandler(std::move(handler3));

        auto handler4 = []() {};
        skeleton->method_without_args_or_return.RegisterHandler(std::move(handler4));

        auto offer_result = skeleton->OfferService();
        if (!offer_result.has_value())
        {
            FailTest("First OfferService failed: ", offer_result.error());
        }

        // Create proxy and verify methods work with first skeleton
        {
            ProxyContainer<EdgeCasesMethodProxy> proxy_container;
            FailTestIf(!proxy_container.CreateProxy(kInstanceSpecifier, kAllMethodNames),
                         "Failed to create proxy for the first skeleton");

            auto result = proxy_container.GetProxy().method_with_args_and_return(kTestValueA, kTestValueB);
            if (!result.has_value())
            {
                FailTest("First skeleton method call failed: ", result.error());
            }
            FailTestIf(*result.value() != kExpectedSum, "First skeleton returned wrong value");
            std::cout << "  First skeleton works correctly" << std::endl;
        }  // Proxy destroyed here

        skeleton->StopOfferService();
    }  // First skeleton destroyed here

    // --- Second skeleton lifecycle (recreation) ---
    {
        std::cout << "  Creating second skeleton after first is destroyed..." << std::endl;
        auto skeleton_result = EdgeCasesMethodSkeleton::Create(kInstanceSpecifier);
        if (!skeleton_result.has_value())
        {
            FailTest("Failed to create second skeleton: ", skeleton_result.error());
        }
        auto skeleton = std::make_unique<EdgeCasesMethodSkeleton>(std::move(skeleton_result).value());

        auto handler1 = [](std::int32_t val1, std::int32_t val2) -> std::int32_t {
            return val1 + val2;
        };
        skeleton->method_with_args_and_return.RegisterHandler(std::move(handler1));

        auto handler2 = [](std::int32_t) {};
        skeleton->method_with_args_only.RegisterHandler(std::move(handler2));

        auto handler3 = []() -> std::int32_t {
            return 42;
        };
        skeleton->method_with_return_only.RegisterHandler(std::move(handler3));

        auto handler4 = []() {};
        skeleton->method_without_args_or_return.RegisterHandler(std::move(handler4));

        auto offer_result = skeleton->OfferService();
        if (!offer_result.has_value())
        {
            FailTest("Second OfferService failed: ", offer_result.error());
        }

        // Create proxy and verify methods work with second skeleton
        {
            ProxyContainer<EdgeCasesMethodProxy> proxy_container;
            FailTestIf(!proxy_container.CreateProxy(kInstanceSpecifier, kAllMethodNames),
                         "Failed to create proxy for the second skeleton");

            auto result = proxy_container.GetProxy().method_with_args_and_return(kTestValueA, kTestValueB);
            if (!result.has_value())
            {
                FailTest("Second skeleton method call failed: ", result.error());
            }
            FailTestIf(*result.value() != kExpectedSum, "Second skeleton returned wrong value");

            std::cout << "  Second skeleton works correctly" << std::endl;
        }  // Proxy destroyed here

        skeleton->StopOfferService();
    }  // Second skeleton destroyed here

    std::cout << "=== Test 4: PASSED ===" << std::endl;
}

enum struct TestType : std::uint8_t
{
    DISABLED_METHOD = 0,
    INCOMPLETE_HANDLERS = 1,
    PROXY_RECREATION = 2,
    SKELETON_RECREATION = 3,
};

auto GetTestType(int argc, const char** argv) -> TestType
{
    auto args = ParseCommandLineArguments(argc, argv, {{"test", ""}});
    auto test_type_str = GetValueIfProvided<std::string>(args, "test");

    FailTestIf(!test_type_str.has_value(), "No test type specified");

    if (test_type_str.value() == "disabled_method_test")
    {
        return TestType::DISABLED_METHOD;
    }
    if (test_type_str.value() == "incomplete_handlers_test")
    {
        return TestType::INCOMPLETE_HANDLERS;
    }
    if (test_type_str.value() == "proxy_recreation_test")
    {
        return TestType::PROXY_RECREATION;
    }
    if (test_type_str.value() == "skeleton_recreation_test")
    {
        return TestType::SKELETON_RECREATION;
    }

    FailTest("Unknown test type");
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
}

}  // namespace
}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    using namespace score::mw::com;
    using namespace score::mw::com::test;

    // Initialize runtime
    SetupAssertHandler();
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    auto test_type = GetTestType(argc, argv);

    switch (test_type)
    {
        case TestType::DISABLED_METHOD:
        {
            TestDisabledMethodReturnsError();
            break;
        }
        case TestType::INCOMPLETE_HANDLERS:
        {
            TestIncompleteHandlers();
            break;
        }
        case TestType::PROXY_RECREATION:
        {
            TestProxyRecreation();
            break;
        }
        case TestType::SKELETON_RECREATION:
        {
            TestSkeletonRecreation();
            break;
        }
        default:
            std::cerr << "Unknown test type" << std::endl;
            return EXIT_FAILURE;
    }

    std::cout << "\n=== ALL EDGE CASES TESTS PASSED ===" << std::endl;
    return EXIT_SUCCESS;
}
