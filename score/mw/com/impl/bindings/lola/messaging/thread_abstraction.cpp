/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/lola/messaging/thread_abstraction.h"

#include <thread>

namespace score::mw::com::impl::lola
{

const ThreadHWConcurrencyIfc* ThreadHWConcurrency::mock_ = nullptr;

std::uint32_t ThreadHWConcurrency::hardware_concurrency() noexcept
{
    if (mock_ != nullptr)
    {
        return mock_->hardware_concurrency();
    }
    return std::thread::hardware_concurrency();
}

}  // namespace score::mw::com::impl::lola
