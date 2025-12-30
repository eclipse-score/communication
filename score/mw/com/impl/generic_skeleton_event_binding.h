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
#include "score/mw/com/impl/bindings/lola/control_slot_composite_indicator.h"
#include "score/result/result.h"

#include <utility>

namespace score::mw::com::impl
{

class GenericSkeletonEventBinding : public SkeletonEventBindingBase
{
  public:
    virtual Result<score::Blank> Send(const void* data) noexcept = 0;

    virtual Result<score::Blank> Send(lola::ControlSlotCompositeIndicator control_slot_indicator) noexcept = 0;

    virtual Result<std::pair<void*, lola::ControlSlotCompositeIndicator>> Allocate() noexcept = 0;

    virtual void Deallocate(lola::ControlSlotCompositeIndicator control_slot_indicator) noexcept = 0;

    virtual std::pair<size_t, size_t> GetSizeInfo() const noexcept = 0;
};

} // namespace score::mw::com::impl