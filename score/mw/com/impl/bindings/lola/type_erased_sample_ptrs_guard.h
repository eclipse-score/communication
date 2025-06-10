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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TYPE_ERASED_SAMPLE_PTRS_GUARD_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TYPE_ERASED_SAMPLE_PTRS_GUARD_H

#include "score/mw/com/impl/tracing/service_element_tracing_data.h"

namespace score::mw::com::impl::lola::tracing
{

/**
 * RAII helper class that will call lola::TracingRuntime::ClearTypeErasedSamplePtrs on destruction.
 */
class TypeErasedSamplePtrsGuard
{
  public:
    explicit TypeErasedSamplePtrsGuard(
        const impl::tracing::ServiceElementTracingData service_element_tracing_data) noexcept;
    ~TypeErasedSamplePtrsGuard() noexcept;

    TypeErasedSamplePtrsGuard(const TypeErasedSamplePtrsGuard&) = delete;
    TypeErasedSamplePtrsGuard& operator=(const TypeErasedSamplePtrsGuard&) = delete;
    TypeErasedSamplePtrsGuard& operator=(TypeErasedSamplePtrsGuard&& other) noexcept = delete;
    TypeErasedSamplePtrsGuard(TypeErasedSamplePtrsGuard&& other) noexcept = delete;

  private:
    impl::tracing::ServiceElementTracingData service_element_tracing_data_;
};

}  // namespace score::mw::com::impl::lola::tracing

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TYPE_ERASED_SAMPLE_PTRS_GUARD_H
