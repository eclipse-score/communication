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
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory_impl.h"
#include "score/mw/com/impl/bindings/lola/generic_skeleton_event.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/plumbing/skeleton_service_element_binding_factory_impl.h"
#include "score/mw/com/impl/skeleton_base.h"

// Updated signature to use DataTypeMetaInfo
score::Result<std::unique_ptr<score::mw::com::impl::GenericSkeletonEventBinding>> 
score::mw::com::impl::GenericSkeletonEventBindingFactoryImpl::Create(
    SkeletonBase& parent, 
    std::string_view event_name, 
    const DataTypeMetaInfo& meta_info) noexcept
{
    const auto& instance_identifier = SkeletonBaseView{parent}.GetAssociatedInstanceIdentifier();
    
    
    return CreateGenericSkeletonServiceElement<GenericSkeletonEventBinding, lola::GenericSkeletonEvent, ServiceElementType::EVENT>(
        instance_identifier, parent, event_name, meta_info);
}