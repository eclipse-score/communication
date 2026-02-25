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
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"

#include "score/result/result.h"

#include <score/span.hpp>
#include <score/stop_token.hpp>

#include <atomic>
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
                const TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info);

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

    TypeErasedCallQueue::TypeErasedElementInfo GetTypeErasedElementInfo() const;

    void SetInArgsAndReturnStorages(std::optional<score::cpp::span<std::byte>> in_args_storage,
                                    std::optional<score::cpp::span<std::byte>> return_storage);

    /// \brief Marks that the ProxyMethod successfully [un]subscribed to its SkeletonMethod
    ///
    /// This helps with error reporting by early returning with an error e.g. if a user calls AllocateInArgs on a method
    /// that was never enabled in Proxy::Create. It is also important to allow us to "disable" a method in the proxy
    /// auto-reconnect case (when the Skeleton has restarted) in case the re-subscription fails.
    void MarkSubscribed();
    void MarkUnsubscribed();

    bool IsSubscribed() const;

  private:
    pid_t skeleton_pid_;
    QualityType asil_level_;
    IRuntime& lola_runtime_;
    TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info_;
    std::optional<score::cpp::span<std::byte>> in_args_storage_;
    std::optional<score::cpp::span<std::byte>> return_storage_;
    ProxyMethodInstanceIdentifier proxy_method_instance_identifier_;

    // is_subscribed_ is an atomic since it may be modified by the FindServiceHandler registered within the Proxy
    std::atomic_bool is_subscribed_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PROXY_METHOD_H
