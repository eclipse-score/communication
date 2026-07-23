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

#ifndef SCORE_MW_COM_TUTORIAL_PROVIDER_SKELETON_COMPONENT_H
#define SCORE_MW_COM_TUTORIAL_PROVIDER_SKELETON_COMPONENT_H

#include "score/mw/com/doc/tutorial/chapter_12/hello_world_service.h"

#include <cstddef>
#include <optional>
#include <utility>

namespace score::mw::com::tutorial
{
class SkeletonComponent
{
  public:
    enum class SendSampleResult
    {
        kHandled,
        kSendFailed,
        kNoSampleAllocated,
        kFatalError,
    };

    explicit SkeletonComponent(HelloWorldSkeleton&& hello_world_skeleton)
        : hello_world_skeleton_(std::move(hello_world_skeleton))
    {
    }

    bool OfferService();
    SendSampleResult SendSample(std::size_t send_counter);

  private:
    std::optional<HelloWorldSkeleton> hello_world_skeleton_;
};

}  // namespace score::mw::com::tutorial

#endif  // SCORE_MW_COM_TUTORIAL_PROVIDER_SKELETON_COMPONENT_H
