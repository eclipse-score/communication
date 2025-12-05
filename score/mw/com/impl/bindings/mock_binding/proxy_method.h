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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_METHOD_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_METHOD_H

#include "score/mw/com/impl/methods/proxy_method_binding.h"

#include <score/assert.hpp>

#include <gmock/gmock.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace score::mw::com::impl::mock_binding
{

class ProxyMethod : public ProxyMethodBinding
{
  public:
    ProxyMethod() : ProxyMethodBinding{{}, {}} {}
    ~ProxyMethod() override = default;

    MOCK_METHOD(score::Result<score::cpp::span<std::byte>>, AllocateInArgs, (std::size_t), (override));
    MOCK_METHOD(score::Result<score::cpp::span<std::byte>>, AllocateReturnType, (std::size_t), (override));
    MOCK_METHOD(ResultBlank, DoCall, (std::size_t), (override));
};

class ProxyMethodFacade : public ProxyMethodBinding
{
  public:
    ProxyMethodFacade(ProxyMethod& proxy_method) : ProxyMethodBinding{{}, {}}, proxy_method_{proxy_method} {}
    ~ProxyMethodFacade() override = default;

    score::Result<score::cpp::span<std::byte>> AllocateInArgs(std::size_t queue_position) override
    {
        return proxy_method_.AllocateInArgs(queue_position);
    }

    score::Result<score::cpp::span<std::byte>> AllocateReturnType(std::size_t queue_position) override
    {
        return proxy_method_.AllocateReturnType(queue_position);
    }

    score::ResultBlank DoCall(std::size_t queue_position) override
    {
        return proxy_method_.DoCall(queue_position);
    }

  private:
    ProxyMethod& proxy_method_;
};

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_METHOD_H
