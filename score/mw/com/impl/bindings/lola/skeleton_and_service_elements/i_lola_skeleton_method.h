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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_AND_SERVICE_ELEMENTS_I_LOLA_SKELETON_METHOD_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_AND_SERVICE_ELEMENTS_I_LOLA_SKELETON_METHOD_H

#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include "score/language/safecpp/scoped_function/scope.h"
#include "score/result/result.h"

#include <sched.h>
#include <score/assert.hpp>
#include <score/callback.hpp>
#include <score/span.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

class ILolaSkeletonMethod
{
  public:
    ILolaSkeletonMethod() = default;
    virtual ~ILolaSkeletonMethod() = default;

    ILolaSkeletonMethod(const ILolaSkeletonMethod&) = delete;
    ILolaSkeletonMethod& operator=(const ILolaSkeletonMethod&) & = delete;
    ILolaSkeletonMethod(ILolaSkeletonMethod&&) noexcept = delete;
    ILolaSkeletonMethod& operator=(ILolaSkeletonMethod&&) & noexcept = delete;

    virtual bool IsRegistered() const = 0;

    virtual void UnregisterMethodCallHandlers() = 0;

    virtual Result<void> OnProxyMethodSubscribeFinished(
        const TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info,
        const std::optional<score::cpp::span<std::byte>> in_arg_queue_storage,
        const std::optional<score::cpp::span<std::byte>> return_queue_storage,
        const ProxyMethodInstanceIdentifier proxy_method_instance_identifier,
        const safecpp::Scope<>& method_call_handler_scope,
        uid_t allowed_proxy_uid,
        pid_t proxy_pid,
        const QualityType asil_level) = 0;

    virtual void OnProxyMethodUnsubscribe(const ProxyMethodInstanceIdentifier proxy_method_instance_identifier) = 0;

    virtual void OnProxyMethodUnsubscribeFinished(
        const ProxyMethodInstanceIdentifier proxy_method_instance_identifier) = 0;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_AND_SERVICE_ELEMENTS_I_LOLA_SKELETON_METHOD_H
