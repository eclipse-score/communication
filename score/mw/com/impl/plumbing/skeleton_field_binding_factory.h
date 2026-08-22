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
#include "score/mw/com/impl/field_tags.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/i_skeleton_field_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory_impl.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include "score/memory/data_type_size_info.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Class that dispatches to either a real SkeletonFieldBindingFactoryImpl or a mocked version
/// SkeletonFieldBindingFactoryMock, if a mock is injected.
class SkeletonFieldBindingFactory final
{
  public:
    /// \brief See documentation in ISkeletonFieldBindingFactory.
    static std::unique_ptr<SkeletonEventBinding> CreateEventBinding(const InstanceIdentifier& identifier,
                                                                    SkeletonBinding& parent_binding,
                                                                    const std::string_view field_name,
                                                                    memory::DataTypeSizeInfo sample_type_size_info,
                                                                    const FieldTagsStore field_tags_store)
    {
        return instance().CreateEventBinding(
            identifier, parent_binding, field_name, sample_type_size_info, field_tags_store);
    }

    /// \brief Inject a mock ISkeletonFieldBindingFactory. If a mock is injected, then all calls on
    /// SkeletonFieldBindingFactory will be dispatched to the mock.
    static void InjectMockBinding(ISkeletonFieldBindingFactory* mock) noexcept
    {
        mock_ = mock;
    }

  private:
    static ISkeletonFieldBindingFactory& instance() noexcept;
    static ISkeletonFieldBindingFactory* mock_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_FIELD_BINDING_FACTORY_H
