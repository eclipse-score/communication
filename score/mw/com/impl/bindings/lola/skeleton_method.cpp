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
    : in_args_type_erased_info_{},
      return_type_type_erased_info_{},
      type_erased_callback_{},
      method_call_handler_scope_{},
      asil_level_{skeleton.GetInstanceQualityType()}
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
    std::weak_ptr<memory::shared::ISharedMemoryResource> methods_shm_resource)
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(type_erased_callback_.has_value(),
                           "Cannot register a method call handler without a registered handler from Register()!");
    auto method_call_callback =
        [this, in_arg_queue_storage, return_queue_storage, type_erased_element_info, methods_shm_resource]() {
            return IMessagePassingService::MethodCallHandler{
                method_call_handler_scope_,
                [this, in_arg_queue_storage, return_queue_storage, type_erased_element_info, methods_shm_resource](
                    std::size_t queue_position) {
                    // Attempt to extend lifetime of methods shm resource so that it's guaranteed to outlive this
                    // method call.
                    const auto methods_shm_resource_shared_ptr = methods_shm_resource.lock();
                    if (methods_shm_resource_shared_ptr == nullptr)
                    {
                        mw::log::LogDebug("lola")
                            << "Methods shared memory region that is used by method call has been "
                               "destroyed. This occurs when the proxy restarted and has created a "
                               "new methods shared memory region. Method will not be called";
                        return;
                    }
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
        }();

    auto& lola_runtime = GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa);
    auto& lola_message_passing = lola_runtime.GetLolaMessaging();
    return lola_message_passing.RegisterMethodCallHandler(
        asil_level_, proxy_method_instance_identifier, std::move(method_call_callback));
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
