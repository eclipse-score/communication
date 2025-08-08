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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_RUNTIME_MOCK_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_RUNTIME_MOCK_H

#include "score/mw/com/impl/bindings/lola/i_runtime.h"

#include "gmock/gmock.h"

namespace score::mw::com::impl::lola
{

/// \brief Mock class for mocking of LoLa binding specific runtime interface.
class RuntimeMock : public IRuntime
{
  public:
    // Suppress "AUTOSAR C++14 M3-9-1" rule. The rule declares:
    // The types used for an object, a function return type, or a function parameter shall be token-for-token identical
    // in all declarations and re-declarations.
    // Mock methods conform interface methods (testing purposes)
    // coverity[autosar_cpp14_m3_9_1_violation]
    MOCK_METHOD(IMessagePassingService&, GetLolaMessaging, (), (noexcept, override));
    // coverity[autosar_cpp14_m3_9_1_violation]
    MOCK_METHOD(bool, HasAsilBSupport, (), (const, noexcept, override));
    // coverity[autosar_cpp14_m3_9_1_violation]
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    // coverity[autosar_cpp14_m3_9_1_violation]
    MOCK_METHOD(ShmSizeCalculationMode, GetShmSizeCalculationMode, (), (const, noexcept, override));
    // coverity[autosar_cpp14_m3_9_1_violation]
    MOCK_METHOD(IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (noexcept, override));
    // coverity[autosar_cpp14_m3_9_1_violation]
    MOCK_METHOD(impl::tracing::ITracingRuntimeBinding*, GetTracingRuntime, (), (noexcept, override));
    // coverity[autosar_cpp14_m3_9_1_violation]
    MOCK_METHOD(RollbackSynchronization&, GetRollbackSynchronization, (), (noexcept, override));
    // coverity[autosar_cpp14_m3_9_1_violation]
    MOCK_METHOD(pid_t, GetPid, (), (const, noexcept, override));
    // coverity[autosar_cpp14_m3_9_1_violation]
    MOCK_METHOD(std::uint32_t, GetApplicationId, (), (const, noexcept, override));
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_RUNTIME_MOCK_H
