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

#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/i_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include "score/memory/data_type_size_info.h"

#include <score/assert.hpp>

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real SkeletonEventBindingFactoryImpl or a mocked version
/// SkeletonEventBindingFactoryMock, if a mock is injected.
class SkeletonEventBindingFactory final
{
  public:
    /// \brief See documentation in ISkeletonEventBindingFactory.
    static std::unique_ptr<SkeletonEventBinding> Create(const InstanceIdentifier& identifier,
                                                        SkeletonBinding& parent_binding,
                                                        const std::string_view event_name,
                                                        memory::DataTypeSizeInfo sample_type_size_info) noexcept
    {
        return instance().Create(identifier, parent_binding, event_name, sample_type_size_info);
    }

    /// \brief Inject a mock ISkeletonEventBindingFactory. If a mock is injected, then all calls on
    /// SkeletonEventBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(ISkeletonEventBindingFactory* mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static ISkeletonEventBindingFactory& instance() noexcept;
    static ISkeletonEventBindingFactory* mock_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_EVENT_BINDING_FACTORY_H
