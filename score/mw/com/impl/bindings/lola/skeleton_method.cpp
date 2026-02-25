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
#include "score/mw/com/impl/bindings/lola/skeleton_method.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/mw/com/impl/runtime.h"

#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/span.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

namespace score::mw::com::impl::lola
{

SkeletonMethod::SkeletonMethod(Skeleton& skeleton, const ElementFqId element_fq_id)
    : in_args_type_erased_info_{},
      return_type_type_erased_info_{},
      type_erased_callback_{},
      registration_guards_{},
      registration_guards_mutex_{}
{
    skeleton.RegisterMethod(element_fq_id.element_id_, *this);
}

ResultBlank SkeletonMethod::RegisterHandler(SkeletonMethodBinding::TypeErasedHandler&& type_erased_callback)
{
    type_erased_callback_ = std::move(type_erased_callback);
    return {};
}

ResultBlank SkeletonMethod::OnProxyMethodSubscribeFinished(
    const TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info,
    const std::optional<score::cpp::span<std::byte>> in_arg_queue_storage,
    const std::optional<score::cpp::span<std::byte>> return_queue_storage,
    const ProxyMethodInstanceIdentifier proxy_method_instance_identifier,
    const safecpp::Scope<>& method_call_handler_scope,
    uid_t allowed_proxy_uid,
    const QualityType asil_level)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(type_erased_callback_.has_value(),
                           "Cannot register a method call handler without a registered handler from Register()!");
    // Note. the scope of the method call handler is owned by the parent Skeleton and will be expired during
    // StopOfferService.
    IMessagePassingService::MethodCallHandler method_call_callback{
        method_call_handler_scope,
        [this, in_arg_queue_storage, return_queue_storage, type_erased_element_info](std::size_t queue_position) {
            std::optional<score::cpp::span<std::byte>> in_args_element_storage{};
            std::optional<score::cpp::span<std::byte>> return_arg_element_storage{};

            if (type_erased_element_info.in_arg_type_info.has_value())
            {
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(in_arg_queue_storage.has_value());
                in_args_element_storage = GetInArgValuesElementStorage(
                    queue_position, in_arg_queue_storage.value(), type_erased_element_info);
            }

            if (type_erased_element_info.return_type_info.has_value())
            {
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(return_queue_storage.has_value());
                return_arg_element_storage = GetReturnValueElementStorage(
                    queue_position, return_queue_storage.value(), type_erased_element_info);
            }

            Call(in_args_element_storage, return_arg_element_storage);
        }};

    auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);
    auto& lola_message_passing = lola_runtime.GetLolaMessaging();
    auto registration_result = lola_message_passing.RegisterMethodCallHandler(
        asil_level, proxy_method_instance_identifier, std::move(method_call_callback), allowed_proxy_uid);
    if (!(registration_result.has_value()))
    {
        return MakeUnexpected<Blank>(registration_result.error());
    }

    const std::lock_guard lock{registration_guards_mutex_};
    registration_guards_.insert({proxy_method_instance_identifier, std::move(registration_result).value()});
    return {};
}

void SkeletonMethod::OnProxyMethodUnsubscribe(const ProxyMethodInstanceIdentifier proxy_method_instance_identifier)
{
    const std::lock_guard lock{registration_guards_mutex_};
    const auto num_elements_erased = registration_guards_.erase(proxy_method_instance_identifier);
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(num_elements_erased != 0U);
}

void SkeletonMethod::UnregisterMethodCallHandlers()
{
    const std::lock_guard lock{registration_guards_mutex_};
    registration_guards_.clear();
}

bool SkeletonMethod::IsRegistered() const
{
    return type_erased_callback_.has_value();
}

void SkeletonMethod::Call(const std::optional<score::cpp::span<std::byte>> in_args,
                          const std::optional<score::cpp::span<std::byte>> return_arg)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(type_erased_callback_.has_value(),
                           "Defensive programming: Call can only be called after OnProxyMethodSubscribeFinished has "
                           "registered the callback with message passing. We check in OnProxyMethodSubscribeFinished "
                           "that type_erased_callback_ has a value.");
    std::invoke(type_erased_callback_.value(), in_args, return_arg);
}

}  // namespace score::mw::com::impl::lola
