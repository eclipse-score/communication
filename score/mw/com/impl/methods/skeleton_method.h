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
#ifndef SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_H
#define SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_H
#include "score/result/result.h"
#include "score/mw/com/impl/methods/skeleton_method_base.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/mw/com/impl/plumbing/skeleton_method_binding_factory.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <tuple>

#include <score/span.hpp>

namespace score::mw::com::impl
{

template <typename Signature>
class SkeletonMethod
{
    static_assert(
        sizeof(Signature) == 0,
        "SkeletonMethod can only be instantiated with a function signature, e.g., SkeletonMethod<void(int, double)>. "
        "You tried to use an unsupported signature type.");
};

template <typename ReturnType, typename... ArgTypes>
class SkeletonMethod<ReturnType(ArgTypes...)> final : public SkeletonMethodBase
{
    static constexpr bool no_in_args_are_pointers = (... && !std::is_pointer_v<ArgTypes>);
    static_assert(no_in_args_are_pointers, "InArgs can not be pointers, since we can not put them in shared memory.");

    static constexpr bool return_value_is_not_a_pointer = (!std::is_pointer_v<ReturnType>);
    static_assert(return_value_is_not_a_pointer,
                  "Return value can not be a pointer, since we can not put them in shared memory.");

  public:
    using MethodType = ReturnType(ArgTypes...);

    SkeletonMethod(SkeletonBase& skeleton_base, const std::string_view method_name)
        : SkeletonMethod(
              skeleton_base,
              method_name,
              SkeletonMethodBindingFactory::Create(SkeletonBaseView{skeleton_base}.GetAssociatedInstanceIdentifier(),
                                                   SkeletonBaseView{skeleton_base}.GetBinding(),
                                                   method_name))
    {
    }

    /// \brief testonly constructor, which allows for direct injection of a mock binding
    SkeletonMethod(SkeletonBase& skeleton_base,
                   const std::string_view method_name,
                   std::unique_ptr<SkeletonMethodBinding> skeleton_method_binding);

    ~SkeletonMethod() = default;

    SkeletonMethod(const SkeletonMethod&) = delete;
    SkeletonMethod& operator=(const SkeletonMethod&) & = delete;

    SkeletonMethod(SkeletonMethod&& other) noexcept;
    SkeletonMethod& operator=(SkeletonMethod&& other) & noexcept;

    /// \brief Register a handler with the binding, which will be executed by the binding when the Proxy calls this
    /// method.
    /// \return score::cpp::blank on success and ComErrc code specified by the binding on failiure
    template <typename Callable>
    ResultBlank RegisterHandler(Callable&& callback);

    void UpdateSkeletonReference(SkeletonBase& skeleton_base) noexcept;
};

template <typename ReturnType, typename... ArgTypes>
SkeletonMethod<ReturnType(ArgTypes...)>::SkeletonMethod(SkeletonBase& skeleton_base,
                                                        const std::string_view method_name,
                                                        std::unique_ptr<SkeletonMethodBinding> skeleton_method_binding)
    : SkeletonMethodBase(skeleton_base, method_name, std::move(skeleton_method_binding))
{
    auto skeleton_base_view = SkeletonBaseView{skeleton_base};
    skeleton_base_view.RegisterMethod(method_name_, *this);
}

template <typename ReturnType, typename... ArgTypes>
SkeletonMethod<ReturnType(ArgTypes...)>::SkeletonMethod(SkeletonMethod&& other) noexcept
    : SkeletonMethodBase(std::move(other))
{
    SkeletonBaseView skeleton_base_view{skeleton_base_.get()};
    skeleton_base_view.UpdateMethod(method_name_, *this);
}

template <typename ReturnType, typename... ArgTypes>
SkeletonMethod<ReturnType(ArgTypes...)>& SkeletonMethod<ReturnType(ArgTypes...)>::operator=(
    SkeletonMethod&& other) & noexcept
{
    if (this != &other)
    {
        SkeletonMethodBase::operator=(std::move(other));
        SkeletonBaseView skeleton_base_view{skeleton_base_.get()};
        skeleton_base_view.UpdateMethod(method_name_, *this);
    }
    return *this;
}

template <typename ReturnType, typename... ArgTypes>
void SkeletonMethod<ReturnType(ArgTypes...)>::UpdateSkeletonReference(SkeletonBase& skeleton_base) noexcept
{
    skeleton_base_ = skeleton_base;
}

template <typename ReturnType, typename... ArgTypes>
template <typename Callable>
ResultBlank SkeletonMethod<ReturnType(ArgTypes...)>::RegisterHandler(Callable&& callback)
{
    static_assert(std::is_rvalue_reference_v<decltype(callback)>,
                  "Callbeck provided to register has to be an rvalue reference");

    // A move would only be problematic if the forwarding refference Callback&& evealuates to an LValue and and is moved
    // from (which the caller of the function might not expect). We static assert that the callback is an RValue
    // refference and can always be movef from.
    // NOLINTNEXTLINE(bugprone-move-forwarding-reference) The callback is asserted to be an RValue reference
    auto callable_invoker = [callable = std::move(callback)](auto&&... ptrs) -> decltype(auto) {
        return std::invoke(callable, (*ptrs)...);
    };

    SkeletonMethodBinding::TypeErasedHandler type_erased_callable =
        [callable_invoker = std::move(callable_invoker)](std::optional<score::cpp::span<std::byte>> type_erased_in_args,
                                                         std::optional<score::cpp::span<std::byte>> type_erased_return) {
            using InArgPtrTuple = std::tuple<ArgTypes*...>;
            InArgPtrTuple typed_in_arg_ptrs{};

            constexpr bool is_in_arg_pack_empty = (sizeof...(ArgTypes) == 0);

            if constexpr (!is_in_arg_pack_empty)
            {
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(type_erased_in_args.has_value(),
                                       "ArgTypes is non void. Thus, type_erased_in_args needs to have a value!");
                typed_in_arg_ptrs = Deserialize<ArgTypes...>(type_erased_in_args.value());
            }

            constexpr bool is_return_type_not_void = !std::is_same_v<ReturnType, void>;
            if constexpr (is_return_type_not_void)
            {
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(type_erased_return.has_value(),
                                       "ReturnType is non void. Thus, type_erased_result needs to have a value!");
                ReturnType res = std::apply(callable_invoker, std::forward<InArgPtrTuple>(typed_in_arg_ptrs));
                SerializeArgs<ReturnType>(type_erased_return.value(), res);
            }
            else
            {
                std::apply(callable_invoker, std::forward<InArgPtrTuple>(typed_in_arg_ptrs));
            }
        };

    return binding_->RegisterHandler(std::move(type_erased_callable));
}

}  // namespace score::mw::com::impl
#endif  // SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_H
