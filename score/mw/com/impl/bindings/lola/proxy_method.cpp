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
#include "score/mw/com/impl/bindings/lola/proxy_method.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/runtime.h"

#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/span.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

ProxyMethod::ProxyMethod(Proxy& proxy,
                         const ElementFqId element_fq_id,
                         const TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info)
    : ProxyMethodBinding{},
      skeleton_pid_{proxy.GetSourcePid()},
      asil_level_{proxy.GetQualityType()},
      lola_runtime_{GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa)},
      type_erased_element_info_{type_erased_element_info},
      in_args_storage_{},
      return_storage_{},
      proxy_method_instance_identifier_{proxy.GetProxyInstanceIdentifier(), element_fq_id.element_id_},
      is_subscribed_{false}
{
    proxy.RegisterMethod(element_fq_id.element_id_, *this);
}

score::Result<score::cpp::span<std::byte>> ProxyMethod::AllocateInArgs(std::size_t queue_position)
{
    if (!is_subscribed_)
    {
        score::mw::log::LogError("lola") << "Trying to allocate in args for a method that was not successfully "
                                          "subscribed. Ensure method enabled in Proxy::Create().";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        type_erased_element_info_.in_arg_type_info.has_value(),
        "AllocateInArgs must only be called when DataTypeSizeInfo is provided for InArg types in the constructor.");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        in_args_storage_.has_value(),
        "AllocateInArgs must only be called when storage is provided for InArg values via SetInArgsAndReturnStorages.");
    return GetInArgValuesElementStorage(queue_position, in_args_storage_.value(), type_erased_element_info_);
}

score::Result<score::cpp::span<std::byte>> ProxyMethod::AllocateReturnType(std::size_t queue_position)
{
    if (!is_subscribed_)
    {
        score::mw::log::LogError("lola") << "Trying to allocate in args for a method that was not successfully "
                                          "subscribed. Ensure method enabled in Proxy::Create().";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        type_erased_element_info_.return_type_info.has_value(),
        "AllocateInArgs must only be called when DataTypeSizeInfo is provided for the Return type in the constructor.");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(return_storage_.has_value(),
                                 "AllocateInArgs must only be called when storage is provided for the Retun value via "
                                 "SetInArgsAndReturnStorages.");
    return GetReturnValueElementStorage(queue_position, return_storage_.value(), type_erased_element_info_);
}

score::ResultBlank ProxyMethod::DoCall(std::size_t queue_position)
{
    if (!is_subscribed_)
    {
        score::mw::log::LogError("lola") << "Trying to call a method that was not successfully subscribed. Ensure method "
                                          "enabled in Proxy::Create().";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    auto& lola_message_passing = lola_runtime_.GetLolaMessaging();
    return lola_message_passing.CallMethod(
        asil_level_, proxy_method_instance_identifier_, queue_position, skeleton_pid_);
}

TypeErasedCallQueue::TypeErasedElementInfo ProxyMethod::GetTypeErasedElementInfo() const
{
    return type_erased_element_info_;
}

void ProxyMethod::SetInArgsAndReturnStorages(std::optional<score::cpp::span<std::byte>> in_args_storage,
                                             std::optional<score::cpp::span<std::byte>> return_storage)
{
    in_args_storage_ = in_args_storage;
    return_storage_ = return_storage;
}

void ProxyMethod::MarkSubscribed()
{
    is_subscribed_ = true;
}

void ProxyMethod::MarkUnsubscribed()
{
    is_subscribed_ = false;
}

bool ProxyMethod::IsSubscribed() const
{
    return is_subscribed_;
}

}  // namespace score::mw::com::impl::lola
