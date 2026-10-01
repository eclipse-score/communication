/*********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#include "score/mw/service/backend/mw_com/proxy_stub.h"

namespace score::mw::service::backend::mw_com
{

std::unique_ptr<FakeProxy::Mock> FakeProxy::gMockInstance{};
Result<FakeProxy> FakeProxy::gFakeProxyCreationResult{FakeProxy{}};

bool operator==(const MockHandle& lhs, const MockHandle& rhs) noexcept
{
    return lhs.instance_id == rhs.instance_id;
}

bool operator<(const MockHandle& lhs, const MockHandle& rhs) noexcept
{
    return lhs.instance_id < rhs.instance_id;
}

}  // namespace score::mw::service::backend::mw_com
