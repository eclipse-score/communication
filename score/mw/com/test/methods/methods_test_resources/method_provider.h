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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_METHOD_PROVIDER_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_METHOD_PROVIDER_H

#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{

/// \brief Helper class which provides provider side method functionality for testing
///
/// This class can be used in conjunction with MethodConsumer for abstracting method-related boilerplate code out of
/// tests. The Skeleton type used to template this class should only contain (zero or more) methods which follow the
/// following structure:
///
///   - Method with InArg and Return
///     - Method variable name: with_in_args_and_return
///     - Signature: `std::int32_t(std::int32_t, std::int32_t)`
///     - Behaviour: returns the sum of the arguments.
///     - Verifications: Consumer verifies that the returned value is the sum of the inputs
///
///   - Method with InArgs only
///     - Method variable name: with_in_args_only
///     - Signature: `void(std::int32_t, std::int32_t)`
///     - Behaviour: Does nothing with provided arguments
///     - Verifications: Provider verifies that the provided args are expected values (expected values provided by
///     individual test)
///
///   - Method with Return only
///     - Method variable name: with_return_only
///     - Signature: `std::int32_t()`
///     - Behaviour: Does nothing
///     - Verifications: Consumer verifies that the return value is as expected (expected value provided by individual
///     test)
///
///   - Method without InArgs / Return
///     - Method variable name: without_args_or_return
///     - Signature: `void()`
///     - Behaviour: Does nothing
///     - Verifications: No verification done.
template <typename Skeleton>
class MethodProvider
{
  public:
    bool CreateSkeleton(InstanceSpecifier instance_specifier);

    bool OfferService();

    bool RegisterMethodHandlerWithInArgsAndReturn();

    bool RegisterMethodHandlerWithInArgsOnly(const std::int32_t expected_input_argument_a,
                                             const std::int32_t expected_input_argument_b);

    bool RegisterMethodHandlerWithReturnOnly(const std::int32_t expected_return_value);

    bool RegisterWithoutInArgsOrReturn();

  private:
    std::unique_ptr<Skeleton> skeleton_;
};

template <typename Skeleton>
bool MethodProvider<Skeleton>::CreateSkeleton(InstanceSpecifier instance_specifier)
{
    auto skeleton_result = Skeleton::Create(std::move(instance_specifier));
    if (!skeleton_result.has_value())
    {
        std::cerr << "Provider: Could not create skeleton: " << skeleton_result.error() << std::endl;
        return false;
    }
    skeleton_ = std::make_unique<Skeleton>(std::move(skeleton_result).value());
    return true;
}

template <typename Skeleton>
bool MethodProvider<Skeleton>::OfferService()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(skeleton_ != nullptr);

    auto offer_result = skeleton_->OfferService();
    if (!(offer_result.has_value()))
    {
        std::cerr << "Provider: Could not offer service: " << offer_result.error() << std::endl;
        return false;
    }

    std::cout << "Provider: Service offered successfully" << std::endl;
    return true;
}

template <typename Skeleton>
bool MethodProvider<Skeleton>::RegisterMethodHandlerWithInArgsAndReturn()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(skeleton_ != nullptr);

    auto handler_with_in_args_and_return = [](std::int32_t a, std::int32_t b) -> std::int32_t {
        std::cout << "Provider: with_in_args_and_return called with " << a << " + " << b << std::endl;
        return a + b;
    };
    const auto register_result =
        skeleton_->with_in_args_and_return.RegisterHandler(std::move(handler_with_in_args_and_return));
    if (!register_result)
    {
        std::cerr << "Provider: Failed to register with_in_args_and_return handler" << std::endl;
        return false;
    }

    std::cout << "Provider: Successfully registered with_in_args_and_return handler" << std::endl;
    return true;
}

template <typename Skeleton>
bool MethodProvider<Skeleton>::RegisterMethodHandlerWithInArgsOnly(const std::int32_t expected_input_argument_a,
                                                                   const std::int32_t expected_input_argument_b)
{
    auto handler_with_in_args_only = [expected_input_argument_a, expected_input_argument_b](std::int32_t a,
                                                                                            std::int32_t b) {
        std::cout << "Provider: with_in_args_only called with " << a << " + " << b << std::endl;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(a == expected_input_argument_a, "Unexpected first InArg received!");
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(b == expected_input_argument_b, "Unexpected second InArg received!");
    };
    const auto register_result = skeleton_->with_in_args_only.RegisterHandler(std::move(handler_with_in_args_only));
    if (!register_result)
    {
        std::cerr << "Provider: Failed to register with_in_args_only handler" << std::endl;
        return false;
    }

    std::cout << "Provider: Successfully registered with_in_args_only handler" << std::endl;
    return true;
}

template <typename Skeleton>
bool MethodProvider<Skeleton>::RegisterMethodHandlerWithReturnOnly(const std::int32_t expected_return_value)
{
    auto handler_with_return_only = [expected_return_value]() -> std::int32_t {
        std::cout << "Provider: with_return_only called. Returning " << expected_return_value << std::endl;
        return expected_return_value;
    };
    const auto register_result = skeleton_->with_return_only.RegisterHandler(std::move(handler_with_return_only));
    if (!register_result)
    {
        std::cerr << "Provider: Failed to register with_return_only handler" << std::endl;
        return false;
    }

    std::cout << "Provider: Successfully registered with_return_only handler" << std::endl;
    return true;
}

template <typename Skeleton>
bool MethodProvider<Skeleton>::RegisterWithoutInArgsOrReturn()
{
    auto handler_without_in_args_or_return = []() {
        std::cout << "Provider: without_args_or_return called." << std::endl;
    };
    const auto register_result =
        skeleton_->without_args_or_return.RegisterHandler(std::move(handler_without_in_args_or_return));
    if (!register_result)
    {
        std::cerr << "Provider: Failed to register without_args_or_return handler" << std::endl;
        return false;
    }

    std::cout << "Provider: Successfully registered without_args_or_return handler" << std::endl;
    return true;
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_METHOD_PROVIDER_H
