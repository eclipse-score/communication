/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_GENERIC_SKELETON_FIELD_H_
#define SCORE_MW_COM_IMPL_GENERIC_SKELETON_FIELD_H_

#include "score/mw/com/impl/generic_skeleton_event.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/skeleton_field_base.h"
#include "score/result/result.h"
#include <score/span.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace score::mw::com::impl
{

class GenericSkeletonField : public SkeletonFieldBase
{
  public:
    GenericSkeletonField(SkeletonBase& skeleton_base,
                         const std::string_view field_name,
                         std::unique_ptr<GenericSkeletonEvent> generic_event,
                         bool has_getter,
                         bool has_setter,
                         bool has_notifier);

    ~GenericSkeletonField() override = default;
    GenericSkeletonField(const GenericSkeletonField&) = delete;
    GenericSkeletonField& operator=(const GenericSkeletonField&) & = delete;

    GenericSkeletonField(GenericSkeletonField&& other) noexcept;
    GenericSkeletonField& operator=(GenericSkeletonField&& other) & noexcept;

    Result<void> Update(score::cpp::span<const uint8_t> raw_value) noexcept;
    Result<void> Update(SampleAllocateePtr<void> sample) noexcept;
    Result<SampleAllocateePtr<void>> Allocate() noexcept;

    Result<void> RegisterGetHandler(std::function<std::vector<uint8_t>()> get_handler);
    Result<void> RegisterSetHandler(std::function<std::vector<uint8_t>(score::cpp::span<const uint8_t>)> set_handler);

  private:
    bool IsInitialValueSaved() const noexcept override;
    Result<void> DoDeferredUpdate() noexcept override;
    bool IsSetHandlerRegistered() const noexcept override;

    GenericSkeletonEvent* GetGenericEvent() const noexcept;

    std::vector<uint8_t> initial_field_value_;
    bool has_initial_value_{false};

    bool has_getter_{false};
    bool has_setter_{false};
    bool has_notifier_{false};
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_SKELETON_FIELD_H_