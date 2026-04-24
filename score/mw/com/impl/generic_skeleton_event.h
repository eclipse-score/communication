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
#ifndef SCORE_MW_COM_IMPL_GENERIC_SKELETON_EVENT_H_
#define SCORE_MW_COM_IMPL_GENERIC_SKELETON_EVENT_H_

#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/result/result.h"

#include <string>

namespace score::mw::com::impl
{

class GenericSkeletonEventBinding;

class GenericSkeletonEvent : public SkeletonEventBase
{
  public:
    GenericSkeletonEvent(SkeletonBase& skeleton_base,
                         const std::string_view event_name,
                         std::unique_ptr<GenericSkeletonEventBinding> binding);

    Result<void> Send(SampleAllocateePtr<void> sample) noexcept;

    Result<SampleAllocateePtr<void>> Allocate() noexcept;

    DataTypeMetaInfo GetSizeInfo() const noexcept;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_SKELETON_EVENT_H_
