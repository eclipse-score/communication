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
#ifndef SCORE_MW_COM_IMPL_RUNTIME_MOCK_H
#define SCORE_MW_COM_IMPL_RUNTIME_MOCK_H

#include "score/mw/com/impl/i_runtime.h"
#include "score/mw/com/impl/i_runtime_binding.h"
#include "score/mw/com/impl/tracing/configuration/i_tracing_filter_config.h"

#include "gmock/gmock.h"

#include <vector>

namespace score::mw::com::impl
{

class RuntimeMock : public IRuntime
{
  public:
    MOCK_METHOD(IRuntimeBinding*, GetBindingRuntime, (BindingType), (const, noexcept, override));
    MOCK_METHOD(std::vector<InstanceIdentifier>, resolve, (const InstanceSpecifier&), (const, override));
    MOCK_METHOD(IServiceDiscovery&, GetServiceDiscovery, (), (noexcept, override));
    MOCK_METHOD(const tracing::ITracingFilterConfig*, GetTracingFilterConfig, (), (const, noexcept, override));
    MOCK_METHOD(tracing::ITracingRuntime*, GetTracingRuntime, (), (const, noexcept, override));
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_RUNTIME_MOCK_H
