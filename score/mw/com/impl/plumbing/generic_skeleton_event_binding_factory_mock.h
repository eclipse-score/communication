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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_MOCK_H
#define SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_MOCK_H

#include "score/mw/com/impl/i_generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/data_type_meta_info.h" 

#include <gmock/gmock.h>

namespace score::mw::com::impl {

class GenericSkeletonEventBindingFactoryMock : public IGenericSkeletonEventBindingFactory {
public:
    MOCK_METHOD(score::Result<std::unique_ptr<GenericSkeletonEventBinding>>, Create, 
        (SkeletonBase&, std::string_view, const DataTypeMetaInfo&), 
        (noexcept, override));
};

} // namespace score::mw::com::impl

#endif // SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_MOCK_H