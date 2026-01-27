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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H

#include "score/mw/com/impl/generic_skeleton_event_binding.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/service_element_type.h" 
#include "score/mw/com/impl/plumbing/skeleton_service_element_binding_factory_impl.h"

#include "score/result/result.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

class GenericSkeletonEventBindingFactory
{
  public:
    static Result<std::unique_ptr<GenericSkeletonEventBinding>> Create(SkeletonBase& skeleton_base,
                                                                       const std::string_view event_name,
                                                                       const DataTypeMetaInfo& size_info) noexcept
    {
        const auto& instance_identifier = SkeletonBaseView{skeleton_base}.GetAssociatedInstanceIdentifier();
        return CreateSkeletonServiceElement<GenericSkeletonEventBinding, lola::GenericSkeletonEvent, ServiceElementType::EVENT>(
            instance_identifier,
            skeleton_base,
            event_name,
            size_info);
    }
};

} // namespace score::mw::com::impl

#endif // SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H