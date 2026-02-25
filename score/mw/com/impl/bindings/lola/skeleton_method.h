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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_METHOD_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_METHOD_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/messaging/method_call_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"

#include "score/language/safecpp/scoped_function/scope.h"
#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/callback.hpp>
#include <score/span.hpp>

#include <cstddef>
#include <optional>
#include <unordered_map>

namespace score::mw::com::impl::lola
{

class Skeleton;

class SkeletonMethod : public SkeletonMethodBinding
{
  public:
    SkeletonMethod(Skeleton& skeleton, const ElementFqId element_fq_id);

    ResultBlank RegisterHandler(SkeletonMethodBinding::TypeErasedHandler&& type_erased_callback) override;

    ResultBlank OnProxyMethodSubscribeFinished(
        const TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info,
        const std::optional<score::cpp::span<std::byte>> in_arg_queue_storage,
        const std::optional<score::cpp::span<std::byte>> return_queue_storage,
        const ProxyMethodInstanceIdentifier proxy_method_instance_identifier,
        const safecpp::Scope<>& method_call_handler_scope,
        uid_t allowed_proxy_uid,
        const QualityType asil_level);

    void OnProxyMethodUnsubscribe(const ProxyMethodInstanceIdentifier proxy_method_instance_identifier);

    bool IsRegistered() const;

    void UnregisterMethodCallHandlers();

  private:
    void Call(const std::optional<score::cpp::span<std::byte>> in_args, const std::optional<score::cpp::span<std::byte>> return_arg);

    std::optional<memory::DataTypeSizeInfo> in_args_type_erased_info_;
    std::optional<memory::DataTypeSizeInfo> return_type_type_erased_info_;
    std::optional<SkeletonMethodBinding::TypeErasedHandler> type_erased_callback_;
    std::unordered_map<ProxyMethodInstanceIdentifier, MethodCallRegistrationGuard> registration_guards_;
    std::mutex registration_guards_mutex_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_METHOD_H
