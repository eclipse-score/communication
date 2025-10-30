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
#include "score/mw/com/test_types.h"

#include "score/mw/com/impl/mocking/test_type_factories.h"
#include "score/mw/com/mocking/i_runtime.h"
#include "score/mw/com/mocking/test_type_factories.h"

namespace score::mw::com
{
namespace runtime
{

void InjectRuntimeMock(IRuntime& runtime_mock)
{
    InjectRuntimeMockImpl(runtime_mock);
}

}  // namespace runtime

InstanceIdentifier MakeDummyInstanceIdentifier(const std::uint16_t unique_identifier)
{
    return impl::MakeFakeInstanceIdentifier(unique_identifier);
}

HandleType MakeDummyHandle(const std::uint16_t unique_identifier)
{
    return impl::MakeFakeHandle(unique_identifier);
}

void ResetInstanceIdentifierConfiguration()
{
    impl::ResetInstanceIdentifierConfiguration();
}

}  // namespace score::mw::com
