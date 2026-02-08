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
#ifndef SCORE_MW_COM_IMPL_I_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_I_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H

#include "score/mw/com/impl/generic_skeleton_event_binding.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/data_type_meta_info.h" 
#include <memory>
#include <string_view>

namespace score::mw::com::impl {

class IGenericSkeletonEventBindingFactory {
public:
    virtual ~IGenericSkeletonEventBindingFactory() noexcept = default;
    
    // Changed SizeInfo -> DataTypeMetaInfo
    virtual score::Result<std::unique_ptr<GenericSkeletonEventBinding>> Create(
        SkeletonBase&, 
        std::string_view, 
        const DataTypeMetaInfo&) noexcept = 0;
};

} // namespace score::mw::com::impl

#endif // SCORE_MW_COM_IMPL_I_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H