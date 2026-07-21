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
#include "score/mw/com/doc/tutorial/chapter_12/consumer/proxy_component.h"

namespace score::mw::com::tutorial
{
bool ProxyComponent::Subscribe()
{
    auto result = hello_world_proxy_->message.Subscribe(1);
    if (!result)
    {
        std::cerr << "Failed to subscribe to HelloWorldService instance 'message' event: " << result.error()
                  << std::endl;
        return false;
    }

    return true;
}

void ProxyComponent::GetNewSamples()
{
    auto get_new_samples_result = hello_world_proxy_->message.GetNewSamples(
        [this](auto&& sample) {
            const auto* buf = sample.Get()->data();
            std::cout << "Sample received. Event \"message\" update received: " << buf << std::endl;
            ++receive_counter_;
        },
        1);
    if (!get_new_samples_result)
    {
        std::cerr << "Failed to get new samples: " << get_new_samples_result.error() << std::endl;
    }
}

size_t ProxyComponent::GetReceiveCounter() const
{
    return receive_counter_;
}

}  // namespace score::mw::com::tutorial
