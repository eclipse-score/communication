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
#include "score/mw/com/impl/bindings/lola/type_erased_sample_ptrs_guard.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/tracing/i_tracing_runtime_binding.h"

namespace score::mw::com::impl::lola::tracing
{

TypeErasedSamplePtrsGuard::TypeErasedSamplePtrsGuard(
    const impl::tracing::ServiceElementTracingData service_element_tracing_data) noexcept
    : service_element_tracing_data_{service_element_tracing_data}
{
}

TypeErasedSamplePtrsGuard::~TypeErasedSamplePtrsGuard() noexcept
{
    const auto* const tracing_runtime = impl::Runtime::getInstance().GetTracingRuntime();
    if (tracing_runtime != nullptr)
    {
        auto& lola_tracing_runtime = tracing_runtime->GetTracingRuntimeBinding(BindingType::kLoLa);
        lola_tracing_runtime.ClearTypeErasedSamplePtrs(service_element_tracing_data_);
    }
}

}  // namespace score::mw::com::impl::lola::tracing
