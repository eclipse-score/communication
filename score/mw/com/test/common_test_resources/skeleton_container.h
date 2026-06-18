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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_SKELETON_CONTAINER_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_SKELETON_CONTAINER_H

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/types.h"

#include <iostream>
#include <memory>
#include <string>

namespace score::mw::com::test
{

template <typename Skeleton>
class SkeletonContainer
{
  public:
    void CreateSkeleton(InstanceSpecifier instance_specifier, const std::string& failure_message_prefix);
    void OfferService(const std::string& failure_message_prefix);

    Skeleton& GetSkeleton()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton_ != nullptr,
                                                    "Skeleton was not successfully created! Cannot get it!");
        return *skeleton_;
    }

    Skeleton&& Extract()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton_ != nullptr,
                                                    "Skeleton was not successfully created! Cannot extract it!");
        return std::move(*skeleton_);
    }

  private:
    std::unique_ptr<Skeleton> skeleton_;
};

template <typename Skeleton>
void SkeletonContainer<Skeleton>::CreateSkeleton(InstanceSpecifier instance_specifier,
                                                 const std::string& failure_message_prefix)
{
    auto skeleton_result = Skeleton::Create(std::move(instance_specifier));
    if (!skeleton_result.has_value())
    {
        FailTest(failure_message_prefix, " Provider: Could not create skeleton: ", skeleton_result.error());
    }
    skeleton_ = std::make_unique<Skeleton>(std::move(skeleton_result).value());
}

template <typename Skeleton>
void SkeletonContainer<Skeleton>::OfferService(const std::string& failure_message_prefix)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(skeleton_ != nullptr);

    auto offer_result = skeleton_->OfferService();
    if (!(offer_result.has_value()))
    {
        FailTest(failure_message_prefix, " Provider: Could not offer service: ", offer_result.error());
    }

    std::cout << "Provider: Service offered successfully" << std::endl;
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_SKELETON_CONTAINER_H
