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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SKELETON_METHOD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_SKELETON_METHOD_BINDING_FACTORY_H

#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/mw/com/impl/plumbing/i_skeleton_method_binding_factory.h"
#include "score/mw/com/impl/skeleton_base.h"

#include <score/assert.hpp>

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real SkeletonMethodBindingFactoryImpl or a mocked version
/// SkeletonMethodBindingFactoryMock, if a mock is injected.
class SkeletonMethodBindingFactory final
{
  public:
    /// \brief See documentation in ISkeletonMethodBindingFactory.
    static std::unique_ptr<SkeletonMethodBinding> Create(const InstanceIdentifier& instance_identifier,
                                                         SkeletonBinding* parent_binding,
                                                         const std::string_view method_name);

    /// \brief Inject a mock ISkeletonMethodBindingFactory. If a mock is injected, then all calls on
    /// SkeletonMethodBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(ISkeletonMethodBindingFactory* mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static ISkeletonMethodBindingFactory& instance();
    static ISkeletonMethodBindingFactory* mock_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_EVENT_BINDING_FACTORY_H
