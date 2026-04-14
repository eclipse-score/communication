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

#ifndef SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_BASE_H
#define SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_BASE_H

#include "score/mw/com/impl/method_identifier.h"
#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"

#include "score/containers/dynamic_array.h"

#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

class ProxyBase;

class ProxyMethodBase
{
  public:
    ProxyMethodBase(ProxyBase& proxy_base,
                    std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                    std::string_view method_name,
                    MethodType method_type = MethodType::kMethod) noexcept
        : proxy_base_{proxy_base},
          method_name_{method_name},
          method_type_{method_type},
          is_return_type_ptr_active_{kCallQueueSize, false},
          binding_{std::move(proxy_method_binding)}
    {
    }
    /// \brief A ProxyMethod shall not be copyable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethodBase(const ProxyMethodBase&) = delete;
    ProxyMethodBase& operator=(const ProxyMethodBase&) = delete;

    /// \brief A ProxyMethod shall be moveable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethodBase(ProxyMethodBase&&) = default;
    ProxyMethodBase& operator=(ProxyMethodBase&&) = default;
    virtual ~ProxyMethodBase() = default;

    void UpdateProxyReference(ProxyBase& proxy_base) noexcept
    {
        proxy_base_ = proxy_base;
    }

  protected:
    /// \brief Size of the call-queue is currently fixed to 1! As soon as we are going to support larger call-queues,
    /// the call-queue-size shall be taken from configuration and handed over to ProxyMethod ctor.
    static constexpr containers::DynamicArray<int>::size_type kCallQueueSize = 1U;

    std::reference_wrapper<ProxyBase> proxy_base_;

    std::string_view method_name_;
    MethodType method_type_;

    /// \brief Dynamic array containing queue-slot active flags: one entry per call-queue position.
    /// \details This array contains bool flags, which indicate, if the return value pointer
    /// returned from a call-operator is active (true), i.e. still in-use by the user or not (false).
    ///
    /// This array is used in these two cases slightly differently:
    /// In the case, that the return type is non-void, the flag indicates, that the return value pointer handed out via
    /// the call-operator for the given call-queue position is still active (true) or not (false).
    /// In the case of a void return type, the flag indicates, that a call at the given call-queue position is still in
    /// progress (true) or not (false). In any case the related queue slot is considered "in-use". As long as we only
    /// support synchronous method calls, the latter case (void return type) doesn't use this array, because "queueing"
    /// (when we had a queue-size > 1) in a synchronous call setup only works for the allocation of in-args (Allocate()
    /// calls), not for the call-operator itself. But since this template specialization has no in-args/Allocate(),
    /// there is also no "queuing" for in-arg allocations. Therefore, in the void-return case, this array will only be
    /// used in a future async call-operator: There it will set the queue-position related flag to "true" at the start
    /// of the async call and back to false, when the asynchronous call concludes.
    containers::DynamicArray<bool> is_return_type_ptr_active_;

    std::unique_ptr<ProxyMethodBinding> binding_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_BASE_H
