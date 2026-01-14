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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_METHOD_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_METHOD_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"

#include "score/result/result.h"

#include <score/span.hpp>
#include <score/stop_token.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

class Proxy;

class ProxyMethod : public ProxyMethodBinding
{
  public:
    ProxyMethod(Proxy& proxy,
                const ElementFqId element_fq_id,
                const std::optional<TypeErasedCallQueue::TypeErasedElementInfo> type_erased_element_info);

    /// \brief Allocates storage for the in-arguments of a method call at the given queue position.
    ///
    /// See ProxyMethodBinding for details
    score::Result<score::cpp::span<std::byte>> AllocateInArgs(std::size_t queue_position) override;

    /// \brief Allocates storage for the return type of a method call at the given queue position.
    ///
    /// See ProxyMethodBinding for details
    score::Result<score::cpp::span<std::byte>> AllocateReturnType(std::size_t queue_position) override;

    /// \brief Performs the actual method call at the given call-queue position.
    ///
    /// See ProxyMethodBinding for details
    score::ResultBlank DoCall(std::size_t queue_position) override;

    std::optional<TypeErasedCallQueue::TypeErasedElementInfo> GetTypeErasedElementInfo() const;

    void SetInArgsAndReturnStorages(std::optional<score::cpp::span<std::byte>> in_args_storage,
                                    std::optional<score::cpp::span<std::byte>> return_storage);

  private:
    IRuntime& lola_runtime_;
    std::optional<TypeErasedCallQueue::TypeErasedElementInfo> type_erased_element_info_;
    std::optional<score::cpp::span<std::byte>> in_args_storage_;
    std::optional<score::cpp::span<std::byte>> return_storage_;
    ProxyInstanceIdentifier proxy_instance_identifier_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_METHOD_H
