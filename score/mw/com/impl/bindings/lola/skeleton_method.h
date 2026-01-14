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
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"

#include "score/language/safecpp/scoped_function/scope.h"
#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/callback.hpp>
#include <score/span.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

class Skeleton;
class SkeletonMethodView;

class SkeletonMethod : public SkeletonMethodBinding
{
    friend class SkeletonMethodView;

  public:
    SkeletonMethod(Skeleton& skeleton, const ElementFqId element_fq_id);

    ResultBlank Register(SkeletonMethodBinding::TypeErasedCallback&& type_erased_callback) override;

  private:
    ResultBlank OnProxyMethodSubscribeFinished(
        const std::optional<TypeErasedCallQueue::TypeErasedElementInfo> type_erased_element_info,
        const std::optional<score::cpp::span<std::byte>> in_arg_values_storage,
        const std::optional<score::cpp::span<std::byte>> return_value_storage,
        const ProxyInstanceIdentifier proxy_instance_identifier);

    void Call(const std::optional<score::cpp::span<std::byte>> in_args, const std::optional<score::cpp::span<std::byte>> return_arg);

    std::optional<memory::DataTypeSizeInfo> in_args_type_erased_info_;
    std::optional<memory::DataTypeSizeInfo> return_type_type_erased_info_;
    std::optional<SkeletonMethodBinding::TypeErasedCallback> type_erased_callback_;
    safecpp::Scope<> method_call_handler_scope_{};
};

class SkeletonMethodView
{
  public:
    SkeletonMethodView(SkeletonMethod& skeleton_method) : skeleton_method_{skeleton_method} {}

    ResultBlank OnProxyMethodSubscribeFinished(
        const std::optional<TypeErasedCallQueue::TypeErasedElementInfo> type_erased_element_info,
        const std::optional<score::cpp::span<std::byte>> in_arg_values_storage,
        const std::optional<score::cpp::span<std::byte>> return_value_storage,
        const ProxyInstanceIdentifier proxy_instance_identifier)
    {
        return skeleton_method_.OnProxyMethodSubscribeFinished(
            type_erased_element_info, in_arg_values_storage, return_value_storage, proxy_instance_identifier);
    }

  private:
    SkeletonMethod& skeleton_method_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_METHOD_H
