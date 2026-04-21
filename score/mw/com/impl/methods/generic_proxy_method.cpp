/********************************************************************************
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
 ********************************************************************************/
#include "score/mw/com/impl/methods/generic_proxy_method.h"

#include "score/mw/com/impl/plumbing/generic_proxy_method_binding_factory.h"

#include <utility>

namespace score::mw::com::impl
{
namespace
{
// The binding's queue-slot plumbing carries over from the typed path, but for now we
// only ever use slot 0 -- the call queue is fixed at size 1.
constexpr std::size_t kSingleQueueSlot{0U};
}  // namespace

GenericProxyMethod::GenericProxyMethod(ProxyBase& parent,
                                       std::unique_ptr<ProxyMethodBinding> binding,
                                       const std::string_view method_name) noexcept
    : ProxyMethodBase(parent, std::move(binding), method_name)
{
    auto proxy_base_view = ProxyBaseView{parent};
    proxy_base_view.RegisterMethod(method_name_, *this);
    if (binding_ == nullptr)
    {
        proxy_base_view.MarkServiceElementBindingInvalid();
    }
}

GenericProxyMethod::GenericProxyMethod(ProxyBase& parent, const std::string_view method_name) noexcept
    : GenericProxyMethod(
          parent,
          GenericProxyMethodBindingFactory::Create(parent.GetHandle(), ProxyBaseView{parent}.GetBinding(), method_name),
          method_name)
{
}

GenericProxyMethod::GenericProxyMethod(GenericProxyMethod&& other) noexcept : ProxyMethodBase(std::move(other))
{
    // Since the address of this method has changed, the entry stored in the parent proxy must be updated.
    ProxyBaseView proxy_base_view{proxy_base_.get()};
    proxy_base_view.UpdateMethod(method_name_, *this);
}

auto GenericProxyMethod::operator=(GenericProxyMethod&& other) noexcept -> GenericProxyMethod&
{
    if (this != &other)
    {
        ProxyMethodBase::operator=(std::move(other));

        ProxyBaseView proxy_base_view{proxy_base_.get()};
        proxy_base_view.UpdateMethod(method_name_, *this);
    }
    return *this;
}

score::Result<score::cpp::span<std::byte>> GenericProxyMethod::AllocateInArgs(const std::size_t queue_position)
{
    return binding_->GetInArgsBuffer(queue_position);
}

score::Result<score::cpp::span<std::byte>> GenericProxyMethod::AllocateReturnType(const std::size_t queue_position)
{
    return binding_->GetReturnValueBuffer(queue_position);
}

score::ResultBlank GenericProxyMethod::DoCall(const std::size_t queue_position)
{
    return binding_->DoCall(queue_position);
}

}  // namespace score::mw::com::impl
