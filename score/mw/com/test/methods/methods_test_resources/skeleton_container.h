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

#include "score/mw/com/types.h"

#include <iostream>
#include <memory>

namespace score::mw::com::test
{

template <typename Skeleton>
class SkeletonContainer
{
  public:
    SkeletonContainer(std::string instance_specifier_string);

    bool CreateSkeleton();
    bool OfferService();

    Skeleton& GetSkeleton()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(skeleton_ != nullptr,
                                                    "Skeleton was not successfully created! Cannot get it!");
        return *skeleton_;
    }

  private:
    std::string instance_specifier_string_;
    std::unique_ptr<Skeleton> skeleton_;
};

template <typename Skeleton>
SkeletonContainer<Skeleton>::SkeletonContainer(std::string instance_specifier_string)
    : instance_specifier_string_{std::move(instance_specifier_string)}
{
}

template <typename Skeleton>
bool SkeletonContainer<Skeleton>::CreateSkeleton()
{
    auto instance_specifier = InstanceSpecifier::Create(std::string{instance_specifier_string_});
    if (!instance_specifier.has_value())
    {
        std::cerr << "Provider: Could not create InstanceSpecifier from: " << instance_specifier_string_ << std::endl;
        return false;
    }

    auto skeleton_result = Skeleton::Create(instance_specifier.value());
    if (!skeleton_result.has_value())
    {
        std::cerr << "Provider: Could not create skeleton: " << skeleton_result.error() << std::endl;
        return false;
    }
    skeleton_ = std::make_unique<Skeleton>(std::move(skeleton_result).value());
    return true;
}

template <typename Skeleton>
bool SkeletonContainer<Skeleton>::OfferService()
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

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_SKELETON_CONTAINER_H
