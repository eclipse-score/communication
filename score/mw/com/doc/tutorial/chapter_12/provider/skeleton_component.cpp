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
#include "score/mw/com/doc/tutorial/chapter_12/provider/skeleton_component.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>

namespace score::mw::com::tutorial
{
namespace
{
constexpr std::string_view kHelloWorld{"Hello World"};
}  // namespace

bool SkeletonComponent::OfferService()
{
    auto offer_result = hello_world_skeleton_->OfferService();
    if (!offer_result)
    {
        std::cerr << "Failed to offer HelloWorldSkeleton instance: " << offer_result.error() << std::endl;
        return false;
    }

    return true;
}

SkeletonComponent::SendSampleResult SkeletonComponent::SendSample(std::size_t send_counter)
{
    auto sample_allocatee_ptr = hello_world_skeleton_->message.Allocate();
    if (!sample_allocatee_ptr)
    {
        std::cerr << "Failed to allocate sample_allocatee_ptr: " << sample_allocatee_ptr.error() << std::endl;
        return SendSampleResult::kNoSampleAllocated;
    }

    const std::string message = std::string{kHelloWorld} + std::to_string(send_counter);
    auto& sample = *sample_allocatee_ptr.value().Get();
    auto* buf = sample.data();
    const auto chars_to_copy = std::min(message.size(), sample.size() - 1U);
    *std::copy_n(message.begin(), chars_to_copy, sample.begin()) = '\0';

    const std::string payload_for_log{buf};
    auto send_result = hello_world_skeleton_->message.Send(std::move(sample_allocatee_ptr.value()));
    if (!send_result)
    {
        std::cerr << "Failed to send sample_allocatee_ptr: " << send_result.error() << std::endl;
        return SendSampleResult::kSendFailed;
    }
    std::cout << "Sample send completed. Event \"message\" update sent: " << payload_for_log << std::endl;

    return SendSampleResult::kHandled;
}

}  // namespace score::mw::com::tutorial
