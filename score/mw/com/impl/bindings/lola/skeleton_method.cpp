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

#include "score/language/safecpp/scoped_function/move_only_scoped_function.h"
#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
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
    : type_erased_callback_{}, method_call_handler_scope_{}
{
    skeleton.RegisterMethod(element_fq_id.element_id_, *this);
}

ResultBlank SkeletonMethod::Register(SkeletonMethodBinding::TypeErasedCallback&& type_erased_callback)
{
    type_erased_callback_ = std::move(type_erased_callback);
    return {};
}

ResultBlank SkeletonMethod::OnProxyMethodSubscribeFinished(
    const std::optional<TypeErasedCallQueue::TypeErasedElementInfo> type_erased_element_info,
    const std::optional<score::cpp::span<std::byte>> in_arg_values_storage,
    const std::optional<score::cpp::span<std::byte>> return_value_storage,
    const ProxyInstanceIdentifier proxy_instance_identifier)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(type_erased_callback_.has_value(),
                           "Cannot register a method call handler without a registered handler from Register()!");
    auto method_call_callback = [this, in_arg_values_storage, return_value_storage, type_erased_element_info]() {
        if (type_erased_element_info.has_value())
        {
            const auto type_erased_element_info_value = type_erased_element_info.value();
            return IMessagePassingService::MethodCallHandler{
                method_call_handler_scope_,
                [this, in_arg_values_storage, return_value_storage, type_erased_element_info_value](
                    std::size_t queue_position) {
                    std::optional<score::cpp::span<std::byte>> in_args_element_storage{};
                    std::optional<score::cpp::span<std::byte>> return_arg_element_storage{};

                    if (type_erased_element_info_value.in_arg_type_info.has_value())
                    {
                        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(in_arg_values_storage.has_value());
                        in_args_element_storage = GetInArgValuesElementStorage(
                            queue_position, in_arg_values_storage.value(), type_erased_element_info_value);
                    }

                    if (type_erased_element_info_value.return_type_info.has_value())
                    {
                        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(return_value_storage.has_value());
                        return_arg_element_storage = GetReturnValueElementStorage(
                            queue_position, return_value_storage.value(), type_erased_element_info_value);
                    }

                    Call(in_arg_values_storage, return_value_storage);
                }};
        }
        else
        {
            return IMessagePassingService::MethodCallHandler{
                method_call_handler_scope_, [this](std::size_t /* queue_position */) {
                    std::optional<score::cpp::span<std::byte>> empty_in_arg_values_storage{};
                    std::optional<score::cpp::span<std::byte>> empty_return_value_storage{};
                    Call(empty_in_arg_values_storage, empty_return_value_storage);
                }};
        }
    }();

    auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);
    auto& lola_message_passing = lola_runtime.GetLolaMessaging();
    return lola_message_passing.RegisterMethodCallHandler(proxy_instance_identifier, std::move(method_call_callback));
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
