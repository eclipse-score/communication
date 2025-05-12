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
#ifndef SCORE_MW_COM_IMPL_TRACING_TRACING_TEST_RESOURCES_H
#define SCORE_MW_COM_IMPL_TRACING_TRACING_TEST_RESOURCES_H

#include "score/mw/com/impl/tracing/i_tracing_runtime_binding.h"
#include "score/mw/com/impl/tracing/tracing_runtime.h"

#include <cstdint>
#include <unordered_map>

namespace score::mw::com::impl::tracing
{

class TracingRuntimeAttorney
{
  public:
    TracingRuntimeAttorney(TracingRuntime& tracing_runtime) noexcept : tracing_runtime_{tracing_runtime} {};

    void SetTracingEnabled(bool enabled) noexcept
    {
        tracing_runtime_.atomic_state_.is_tracing_enabled = enabled;
    }

    std::uint32_t GetFailureCounter() const noexcept
    {
        return tracing_runtime_.atomic_state_.consecutive_failure_counter;
    }
    void SetFailureCounter(std::uint32_t counter) noexcept
    {
        tracing_runtime_.atomic_state_.consecutive_failure_counter = counter;
    }

    std::unordered_map<BindingType, ITracingRuntimeBinding*>& GetTracingRuntimeBindings()
    {
        return tracing_runtime_.tracing_runtime_bindings_;
    }

  private:
    TracingRuntime& tracing_runtime_;
};

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_TRACING_TEST_RESOURCES_H
