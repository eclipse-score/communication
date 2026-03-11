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
#ifndef SCORE_MW_COM_MOCKING_RUNTIME_MOCK_IMPL_H
#define SCORE_MW_COM_MOCKING_RUNTIME_MOCK_IMPL_H

#include "score/mw/com/mocking/i_runtime.h"

#include <gmock/gmock.h>

namespace score::mw::com::runtime
{

class RuntimeMock : public IRuntime
{
  public:
    MOCK_METHOD(score::Result<InstanceIdentifierContainer>, ResolveInstanceIDs, (const InstanceSpecifier), (override));
    MOCK_METHOD(void, InitializeRuntime, (const std::int32_t, score::cpp::span<const score::StringLiteral>), (override));
    MOCK_METHOD(void, InitializeRuntime, (const runtime::RuntimeConfiguration&), (override));
};

}  // namespace score::mw::com::runtime

#endif  // SCORE_MW_COM_MOCKING_RUNTIME_MOCK_IMPL_H
