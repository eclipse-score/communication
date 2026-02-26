/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional information
 * regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#pragma once

#include "score/mw/com/impl/skeleton_event_binding.h"

#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/result/result.h"

#include <utility>
#include <cstddef>

namespace score::mw::com::impl
{

class GenericSkeletonEventBinding : public SkeletonEventBindingBase
{
  public:
    virtual Result<score::Blank> Send(SampleAllocateePtr<void> sample) noexcept = 0;

    virtual Result<SampleAllocateePtr<void>> Allocate() noexcept = 0;

    virtual std::pair<size_t, size_t> GetSizeInfo() const noexcept = 0;
};

} // namespace score::mw::com::impl