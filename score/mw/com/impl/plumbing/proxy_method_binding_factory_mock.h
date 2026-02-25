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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_MOCK_H
#define SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_MOCK_H

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/plumbing/i_proxy_method_binding_factory.h"
#include "score/mw/com/impl/proxy_binding.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl
{

class ProxyMethodBindingFactoryMock : public IProxyMethodBindingFactory
{
  public:
    MOCK_METHOD(std::unique_ptr<ProxyMethodBinding>,
                Create,
                (HandleType parent_handle, ProxyBinding* parent_binding, const std::string_view method_name),
                (noexcept, override));
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_PROXY_METHOD_BINDING_FACTORY_MOCK_H
