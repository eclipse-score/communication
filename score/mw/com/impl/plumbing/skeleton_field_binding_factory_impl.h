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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SKELETON_FIELD_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_SKELETON_FIELD_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/field_tags_store.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/i_skeleton_field_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_service_element_binding_factory_impl.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
class SkeletonFieldBindingFactoryImpl : public ISkeletonFieldBindingFactory
{
  public:
    std::unique_ptr<SkeletonEventBinding> CreateEventBinding(const InstanceIdentifier& identifier,
                                                             SkeletonBinding& parent_binding,
                                                             const std::string_view field_name,
                                                             memory::DataTypeSizeInfo sample_type_size_info,
                                                             const FieldTagsStore field_tags_store) noexcept override;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_FIELD_BINDING_FACTORY_IMPL_H
