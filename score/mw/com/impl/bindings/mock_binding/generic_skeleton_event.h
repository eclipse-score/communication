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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_GENERIC_SKELETON_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_GENERIC_SKELETON_EVENT_H

#include "score/mw/com/impl/generic_skeleton_event_binding.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::mock_binding
{

class GenericSkeletonEvent : public GenericSkeletonEventBinding
{
  public:
    //  Use explicit 'score::mw::com::impl::SampleAllocateePtr' to avoid ambiguity 
    // with the 'mock_binding::SampleAllocateePtr' alias (which is a unique_ptr).
    
    MOCK_METHOD(Result<score::Blank>, Send, 
        (score::mw::com::impl::SampleAllocateePtr<void>), 
        (noexcept, override));

    MOCK_METHOD(Result<score::mw::com::impl::SampleAllocateePtr<void>>, Allocate, 
        (), 
        (noexcept, override));

    MOCK_METHOD((std::pair<size_t, size_t>), GetSizeInfo, (), (const, noexcept, override));
    MOCK_METHOD(ResultBlank, PrepareOffer, (), (noexcept, override));
    MOCK_METHOD(void, PrepareStopOffer, (), (noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(void, SetSkeletonEventTracingData, (impl::tracing::SkeletonEventTracingData), (noexcept, override));
    MOCK_METHOD(std::size_t, GetMaxSize, (), (const, noexcept, override));
};

} // namespace score::mw::com::impl::mock_binding

#endif // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_GENERIC_SKELETON_EVENT_H