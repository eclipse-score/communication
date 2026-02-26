/*******************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROVIDER_RESOURCES_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROVIDER_RESOURCES_H

#include "score/mw/com/com_error_domain.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/types.h"

#include "score/result/result.h"

#include <iostream>
#include <string_view>
#include <utility>

namespace score::mw::com::test
{

template <typename SkeletonInterface>
Result<SkeletonInterface> CreateSkeleton(const std::string_view message_prefix,
                                         const std::string_view instance_specifier_string_view,
                                         CheckPointControl& check_point_control) noexcept
{
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{instance_specifier_string_view});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << message_prefix << ": Could not create instance specifier due to error "
                  << instance_specifier_result.error() << ", exiting!\n";
        check_point_control.ErrorOccurred();
        return MakeUnexpected<SkeletonInterface>(std::move(instance_specifier_result).error());
    }

    std::cerr << message_prefix << ": Before Skeleton Creation." << std::endl;
    auto skeleton_wrapper_result = SkeletonInterface::Create(std::move(instance_specifier_result).value());
    if (!skeleton_wrapper_result.has_value())
    {
        std::cerr << message_prefix << ": Unable to construct skeleton: " << skeleton_wrapper_result.error()
                  << ", exiting !\n ";
        check_point_control.ErrorOccurred();
        return MakeUnexpected<SkeletonInterface>(std::move(skeleton_wrapper_result).error());
    }
    std::cerr << message_prefix << ": Successfully created lola skeleton" << std::endl;
    return skeleton_wrapper_result;
}

template <typename SkeletonInterface>
ResultBlank OfferService(const std::string_view message_prefix,
                         SkeletonInterface& skeleton,
                         CheckPointControl& check_point_control) noexcept
{
    const auto offer_service_result = skeleton.OfferService();
    std::cerr << message_prefix << ": After Skeleton Offered." << std::endl;
    if (!offer_service_result.has_value())
    {
        std::cerr << message_prefix << ": Unable to offer service: " << offer_service_result.error() << ", exiting!\n";
        check_point_control.ErrorOccurred();
        return offer_service_result;
    }
    std::cerr << message_prefix << ": Service instance is offered." << std::endl;
    return {};
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROVIDER_RESOURCES_H
