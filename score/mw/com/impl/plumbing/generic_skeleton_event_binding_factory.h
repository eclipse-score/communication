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

#include "score/mw/com/impl/i_generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/generic_skeleton_event_binding.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/data_type_meta_info.h"
#include "score/mw/com/impl/service_element_type.h" 
#include "score/mw/com/impl/plumbing/skeleton_service_element_binding_factory_impl.h"
#include "score/mw/com/impl/bindings/lola/generic_skeleton_event.h" 

#include "score/result/result.h"

#include <memory>
#include <string_view>


namespace score::mw::com::impl
{
  
class GenericSkeletonEventBindingFactory
{
  public:
    // C++17 inline static allows defining this variable directly in the header.
    // This serves as the "hook" for your unit tests.
    inline static IGenericSkeletonEventBindingFactory* mock_ = nullptr;

    // This static method allows your Source Code (generic_skeleton.cpp) 
    // to call GenericSkeletonEventBindingFactory::Create(...) directly.
    static Result<std::unique_ptr<GenericSkeletonEventBinding>> Create(
        SkeletonBase& skeleton_base,
        std::string_view event_name,
        const DataTypeMetaInfo& meta_info) noexcept 
    {
        // A. If a Mock is registered (during Unit Tests), use it.
        if (mock_ != nullptr)
        {
            //  Pass meta_info to mock
            return mock_->Create(skeleton_base, event_name, meta_info);
        }

        // B. Otherwise (in Production), use the Real Implementation.
        const auto& instance_identifier = SkeletonBaseView{skeleton_base}.GetAssociatedInstanceIdentifier();
        
      
        return CreateGenericSkeletonServiceElement<GenericSkeletonEventBinding, lola::GenericSkeletonEvent, ServiceElementType::EVENT>(
            instance_identifier,
            skeleton_base,
            event_name,
            meta_info); 
    }
};


} // namespace score::mw::com::impl

#endif // SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H