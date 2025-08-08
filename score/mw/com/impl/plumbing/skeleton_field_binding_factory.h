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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SKELETON_FIELD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_SKELETON_FIELD_BINDING_FACTORY_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/i_skeleton_field_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory_impl.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real SkeletonFieldBindingFactoryImpl or a mocked version
/// SkeletonFieldBindingFactoryMock, if a mock is injected.
template <typename SampleType>
class SkeletonFieldBindingFactory final
{
  public:
    /// \brief See documentation in ISkeletonFieldBindingFactory.
    static std::unique_ptr<SkeletonEventBinding<SampleType>> CreateEventBinding(const InstanceIdentifier& identifier,
                                                                                SkeletonBase& parent,
                                                                                const std::string_view field_name)
    {
        return instance().CreateEventBinding(identifier, parent, field_name);
    }

    /// \brief Inject a mock ISkeletonFieldBindingFactory. If a mock is injected, then all calls on
    /// SkeletonFieldBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(ISkeletonFieldBindingFactory<SampleType>* mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static ISkeletonFieldBindingFactory<SampleType>& instance() noexcept;
    static ISkeletonFieldBindingFactory<SampleType>* mock_;
};

template <typename SampleType>
auto SkeletonFieldBindingFactory<SampleType>::instance() noexcept -> ISkeletonFieldBindingFactory<SampleType>&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }

    // Suppress "AUTOSAR C++14 A3-2-2", The rule states: "Static and thread-local objects shall be constant-initialized"
    // It cannot be made const since we will need to call non-const methods from a static instance.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static SkeletonFieldBindingFactoryImpl<SampleType> instance{};
    return instance;
}

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file in multiple
// translation units without violating the One Definition Rule."
// The static member mock_ in the SkeletonFieldBindingFactory is intentionally defined in the header file
// to facilitate template instantiation across multiple translation units used in diff applications.
template <typename SampleType>
// coverity[autosar_cpp14_a3_1_1_violation]
ISkeletonFieldBindingFactory<SampleType>* SkeletonFieldBindingFactory<SampleType>::mock_{nullptr};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_FIELD_BINDING_FACTORY_H
