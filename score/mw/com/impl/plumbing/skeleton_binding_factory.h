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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SKELETON_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_SKELETON_BINDING_FACTORY_H

#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/i_skeleton_binding_factory.h"
#include "score/mw/com/impl/skeleton_binding.h"

#include <score/span.hpp>

#include <memory>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real SkeletonBindingFactoryImpl or a mocked version
/// SkeletonBindingFactoryMock, if a mock is injected.
class SkeletonBindingFactory final
{
  public:
    /// \brief See documentation in ISkeletonBindingFactory.
    static std::unique_ptr<SkeletonBinding> Create(const InstanceIdentifier& instance_identifier) noexcept
    {
        return instance().Create(instance_identifier);
    }

    /// \brief Inject a mock ISkeletonBindingFactory. If a mock is injected, then all calls on SkeletonBindingFactory
    /// will be dispatched to the mock.
    static void InjectMockBinding(ISkeletonBindingFactory* mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static ISkeletonBindingFactory& instance() noexcept;
    static ISkeletonBindingFactory* mock_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_BINDING_FACTORY_H
