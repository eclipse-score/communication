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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_BINDING_FACTORY_MOCK_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_BINDING_FACTORY_MOCK_H

#include "score/mw/com/impl/plumbing/i_proxy_binding_factory.h"

#include <gmock/gmock.h>

#include <memory>
#include <vector>

namespace score::mw::com::impl
{

class ProxyBindingFactoryMock : public IProxyBindingFactory
{
  public:
    MOCK_METHOD(std::unique_ptr<ProxyBinding>, Create, (const HandleType&), (noexcept, override));
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_BINDING_FACTORY_MOCK_H
