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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SKELETON_EVENT_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_SKELETON_EVENT_BINDING_FACTORY_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/i_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory_impl.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include <score/assert.hpp>

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real SkeletonEventBindingFactoryImpl or a mocked version
/// SkeletonEventBindingFactoryMock, if a mock is injected.
template <typename SampleType>
class SkeletonEventBindingFactory final
{
  public:
    /// \brief See documentation in ISkeletonEventBindingFactory.
    static std::unique_ptr<SkeletonEventBinding<SampleType>> Create(const InstanceIdentifier& identifier,
                                                                    SkeletonBase& parent,
                                                                    const std::string_view event_name) noexcept
    {
        return instance().Create(identifier, parent, event_name);
    }

    /// \brief Inject a mock ISkeletonEventBindingFactory. If a mock is injected, then all calls on
    /// SkeletonEventBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(ISkeletonEventBindingFactory<SampleType>* mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static ISkeletonEventBindingFactory<SampleType>& instance() noexcept;
    static ISkeletonEventBindingFactory<SampleType>* mock_;
};

template <typename SampleType>
auto SkeletonEventBindingFactory<SampleType>::instance() noexcept -> ISkeletonEventBindingFactory<SampleType>&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }

    // Suppress "AUTOSAR C++14 A3-3-2", The rule states: "Static and thread-local objects shall be constant-initialized"
    // It cannot be made const since we will need to call non-const methods from a static instance.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static SkeletonEventBindingFactoryImpl<SampleType> instance{};
    return instance;
}

// Suppress "AUTOSAR_C++14_A3-1-1" rule finding. This rule states:" It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule".
template <typename SampleType>
// coverity[autosar_cpp14_a3_1_1_violation] This is false-positive, "mock_" is defined once
ISkeletonEventBindingFactory<SampleType>* SkeletonEventBindingFactory<SampleType>::mock_ = nullptr;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_EVENT_BINDING_FACTORY_H
