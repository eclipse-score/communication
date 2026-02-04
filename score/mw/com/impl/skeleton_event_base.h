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
#ifndef SCORE_MW_COM_IMPL_SKELETON_EVENT_BASE_H
#define SCORE_MW_COM_IMPL_SKELETON_EVENT_BASE_H

#include "score/mw/com/impl/flag_owner.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"

#include "score/result/result.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

class SkeletonEventBaseView;
// False Positive: this is a normal forward declaration.
// Which is used to avoid cyclic dependency with skeleton_base.h
// coverity[autosar_cpp14_m3_2_3_violation]
class SkeletonBase;

class SkeletonEventBase
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a view to the private members of this class.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend SkeletonEventBaseView;

  public:
    SkeletonEventBase(SkeletonBase& skeleton_base,
                      const std::string_view event_name,
                      std::unique_ptr<SkeletonEventBindingBase> binding)
        : binding_{std::move(binding)},
          skeleton_base_{skeleton_base},
          event_name_{event_name},
          tracing_data_{},
          service_offered_flag_{}
    {
    }

    virtual ~SkeletonEventBase() = default;

    void UpdateSkeletonReference(SkeletonBase& skeleton_base) noexcept
    {
        skeleton_base_ = skeleton_base;
    }

    /// \brief Used to indicate that the event shall be available to consumer
    /// Performs binding independent functionality and then dispatches to the binding
    score::ResultBlank PrepareOffer() noexcept
    {
        const auto result = binding_->PrepareOffer();
        if (result.has_value())
        {
            service_offered_flag_.Set();
        }
        return result;
    }

    /// \brief Used to indicate that the event shall no longer be available to consumer
    /// Performs binding independent functionality and then dispatches to the binding
    void PrepareStopOffer() noexcept
    {
        if (service_offered_flag_.IsSet())
        {
            binding_->PrepareStopOffer();
            service_offered_flag_.Clear();
        }
    }

  protected:
    SkeletonEventBase(const SkeletonEventBase&) = delete;
    SkeletonEventBase& operator=(const SkeletonEventBase&) & = delete;

    SkeletonEventBase(SkeletonEventBase&&) noexcept = default;
    SkeletonEventBase& operator=(SkeletonEventBase&& other) & noexcept = default;

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to exchange this information between the SkeletonEventBase and the
    // SkeletonEvent.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::unique_ptr<SkeletonEventBindingBase> binding_;

    // The SkeletonEventBase must contain a reference to the SkeletonBase so that a SkeletonBase can call
    // UpdateSkeletonReference whenever it is moved to a new address. A SkeletonBase only has a reference to a
    // SkeletonEventBase, not a typed SkeletonEvent, which is why UpdateSkeletonReference has to be in this class
    // despite skeleton_base_ being used in the derived class, SkeletonEvent.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::reference_wrapper<SkeletonBase> skeleton_base_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::string_view event_name_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    tracing::SkeletonEventTracingData tracing_data_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    FlagOwner service_offered_flag_;
};

class SkeletonEventBaseView
{
  public:
    explicit SkeletonEventBaseView(SkeletonEventBase& skeleton_event_base) : skeleton_event_base_{skeleton_event_base}
    {
    }

    SkeletonEventBindingBase* GetBinding()
    {
        return skeleton_event_base_.binding_.get();
    }

    const tracing::SkeletonEventTracingData& GetSkeletonEventTracing()
    {
        return skeleton_event_base_.tracing_data_;
    }

    void SetSkeletonEventTracing(const tracing::SkeletonEventTracingData& skeleton_event_tracing_data)
    {
        skeleton_event_base_.tracing_data_ = skeleton_event_tracing_data;
    }

  private:
    SkeletonEventBase& skeleton_event_base_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_EVENT_BASE_H
