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
#ifndef SCORE_MW_COM_IMPL_TRACING_TRACING_RUNTIME_MOCK_H
#define SCORE_MW_COM_IMPL_TRACING_TRACING_RUNTIME_MOCK_H

#include "score/mw/com/impl/tracing/i_tracing_runtime.h"

#include "gmock/gmock.h"

namespace score::mw::com::impl::tracing
{

class TracingRuntimeMock : public ITracingRuntime
{
  public:
    MOCK_METHOD(bool, IsTracingEnabled, (), (noexcept, override));
    MOCK_METHOD(void, DisableTracing, (), (noexcept, override));
    MOCK_METHOD(ServiceElementTracingData, RegisterServiceElement, (BindingType, uint8_t), (noexcept, override));
    MOCK_METHOD(void, SetDataLossFlag, (BindingType), (noexcept, override));
    MOCK_METHOD(void,
                RegisterShmObject,
                (BindingType, ServiceElementInstanceIdentifierView, analysis::tracing::ShmObjectHandle, void*),
                (noexcept, override));
    MOCK_METHOD(void, UnregisterShmObject, (BindingType, ServiceElementInstanceIdentifierView), (noexcept, override));

    MOCK_METHOD(ResultBlank,
                Trace,
                (BindingType,
                 ServiceElementTracingData service_element_tracing_data,
                 ServiceElementInstanceIdentifierView,
                 TracePointType,
                 TracePointDataId,
                 TypeErasedSamplePtr sample_ptr,
                 const void*,
                 std::size_t),
                (noexcept, override));
    MOCK_METHOD(ResultBlank,
                Trace,
                (BindingType,
                 ServiceElementInstanceIdentifierView,
                 TracePointType,
                 score::cpp::optional<TracePointDataId>,
                 const void*,
                 std::size_t),
                (noexcept, override));
    MOCK_METHOD(ITracingRuntimeBinding&, GetTracingRuntimeBinding, (BindingType), (const, noexcept, override));
};

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_TRACING_RUNTIME_MOCK_H
