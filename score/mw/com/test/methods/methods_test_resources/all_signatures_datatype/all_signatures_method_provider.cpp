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
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_method_provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"

namespace score::mw::com::test
{

void AllSignaturesMethodProvider::CreateSkeleton(InstanceSpecifier instance_specifier,
                                                 const std::string& failure_message_prefix)
{
    skeleton_container_.CreateSkeleton(std::move(instance_specifier), failure_message_prefix);
}

void AllSignaturesMethodProvider::OfferService(const std::string& failure_message_prefix)
{
    skeleton_container_.OfferService(failure_message_prefix);
}

AllSignaturesSkeleton& AllSignaturesMethodProvider::GetSkeleton()
{
    return skeleton_container_.GetSkeleton();
}

void AllSignaturesMethodProvider::RegisterMethodHandlerWithInArgsAndReturn(const std::string& failure_message_prefix)
{
    auto handler_with_in_args_and_return = [](std::int32_t a, std::int32_t b) -> std::int32_t {
        std::cout << "Provider: with_in_args_and_return called with " << a << " + " << b << std::endl;
        return a + b;
    };
    const auto register_result = skeleton_container_.GetSkeleton().with_in_args_and_return.RegisterHandler(
        std::move(handler_with_in_args_and_return));
    if (!register_result)
    {
        FailTest(failure_message_prefix, " Provider: Failed to register with_in_args_and_return handler");
    }

    std::cout << "Provider: Successfully registered with_in_args_and_return handler" << std::endl;
}

void AllSignaturesMethodProvider::RegisterMethodHandlerWithInArgsOnly(const std::int32_t expected_input_argument_a,
                                                                      const std::int32_t expected_input_argument_b,
                                                                      const std::string& failure_message_prefix)
{
    auto handler_with_in_args_only = [expected_input_argument_a, expected_input_argument_b](std::int32_t a,
                                                                                            std::int32_t b) {
        std::cout << "Provider: with_in_args_only called with " << a << " + " << b << std::endl;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(a == expected_input_argument_a, "Unexpected first InArg received!");
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(b == expected_input_argument_b, "Unexpected second InArg received!");
    };
    const auto register_result =
        skeleton_container_.GetSkeleton().with_in_args_only.RegisterHandler(std::move(handler_with_in_args_only));
    if (!register_result)
    {
        FailTest(failure_message_prefix, " Provider: Failed to register with_in_args_only handler");
    }

    std::cout << "Provider: Successfully registered with_in_args_only handler" << std::endl;
}

void AllSignaturesMethodProvider::RegisterMethodHandlerWithReturnOnly(const std::int32_t expected_return_value,
                                                                      const std::string& failure_message_prefix)
{
    auto handler_with_return_only = [expected_return_value]() -> std::int32_t {
        std::cout << "Provider: with_return_only called. Returning " << expected_return_value << std::endl;
        return expected_return_value;
    };
    const auto register_result =
        skeleton_container_.GetSkeleton().with_return_only.RegisterHandler(std::move(handler_with_return_only));
    if (!register_result)
    {
        FailTest(failure_message_prefix, " Provider: Failed to register with_return_only handler");
    }

    std::cout << "Provider: Successfully registered with_return_only handler" << std::endl;
}

void AllSignaturesMethodProvider::RegisterWithoutInArgsOrReturn(const std::string& failure_message_prefix)
{
    auto handler_without_in_args_or_return = []() {
        std::cout << "Provider: without_args_or_return called." << std::endl;
    };
    const auto register_result = skeleton_container_.GetSkeleton().without_args_or_return.RegisterHandler(
        std::move(handler_without_in_args_or_return));
    if (!register_result)
    {
        FailTest(failure_message_prefix, " Provider: Failed to register without_args_or_return handler");
    }

    std::cout << "Provider: Successfully registered without_args_or_return handler" << std::endl;
}

}  // namespace score::mw::com::test
