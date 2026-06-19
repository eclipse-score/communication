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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_ALL_SIGNATURES_DATA_TYPE_ALL_SIGNATURES_METHOD_PROVIDER_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_ALL_SIGNATURES_DATA_TYPE_ALL_SIGNATURES_METHOD_PROVIDER_H

#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_datatype.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>
#include <string>

namespace score::mw::com::test
{

/// \brief Helper class which provides provider side method functionality for testing
///
/// This class can be used in conjunction with MethodConsumer for abstracting method-related boilerplate code out of
/// tests. The Skeleton type is generated from the AllSignaturesInterface.
class AllSignaturesMethodProvider
{
  public:
    void CreateSkeleton(InstanceSpecifier instance_specifier, const std::string& failure_message_prefix);

    void OfferService(const std::string& failure_message_prefix);

    AllSignaturesSkeleton& GetSkeleton();

    void RegisterMethodHandlerWithInArgsAndReturn(const std::string& failure_message_prefix);

    void RegisterMethodHandlerWithInArgsOnly(const std::int32_t expected_input_argument_a,
                                             const std::int32_t expected_input_argument_b,
                                             const std::string& failure_message_prefix);

    void RegisterMethodHandlerWithReturnOnly(const std::int32_t expected_return_value,
                                             const std::string& failure_message_prefix);

    void RegisterWithoutInArgsOrReturn(const std::string& failure_message_prefix);

  private:
    SkeletonContainer<AllSignaturesSkeleton> skeleton_container_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_ALL_SIGNATURES_DATA_TYPE_ALL_SIGNATURES_METHOD_PROVIDER_H
