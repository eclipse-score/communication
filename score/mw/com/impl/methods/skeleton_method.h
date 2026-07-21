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

#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/methods/method_traits_checker.h"
#include "score/mw/com/impl/methods/skeleton_method_base.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/mw/com/impl/plumbing/skeleton_method_binding_factory.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/util/type_erased_storage.h"
#include "score/result/result.h"

#include <functional>
#include <memory>
#include <optional>
#include <tuple>

#include <score/span.hpp>

namespace score::mw::com::impl
{

namespace detail
{

/// \brief Invokes `callable`, forwarding `quality_type` only if the callable accepts it as its first argument.
template <typename Callable, typename... Args>
void InvokeWithOptionalQuality([[maybe_unused]] QualityType quality_type, Callable& callable, Args&&... args)
{
    if constexpr (std::is_invocable_v<Callable&, QualityType, Args&...>)
    {
        std::invoke(callable, quality_type, std::forward<Args>(args)...);
    }
    else
    {
        std::invoke(callable, std::forward<Args>(args)...);
    }
}

}  // namespace detail

template <typename, typename...>
class SkeletonField;

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
    template <typename, typename...>
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonFieldImpl;

    struct FieldOnlyConstructorEnabler
    {
    };

  public:
    using MethodType = ReturnType(ArgTypes...);

    SkeletonMethod(SkeletonBase& skeleton_base, const std::string_view method_name)
        : SkeletonMethod(
              skeleton_base,
              method_name,
              SkeletonMethodBindingFactory::Create(SkeletonBaseView{skeleton_base}.GetAssociatedInstanceIdentifier(),
                                                   SkeletonBaseView{skeleton_base}.GetBinding(),
                                                   method_name,
                                                   ::score::mw::com::impl::MethodType::kMethod),
              ::score::mw::com::impl::MethodType::kMethod)
    {
    }

    SkeletonMethod(SkeletonBase& skeleton_base,
                   const std::string_view method_name,
                   ::score::mw::com::impl::MethodType method_type,
                   FieldOnlyConstructorEnabler) noexcept
        : SkeletonMethodBase(
              method_name,
              SkeletonMethodBindingFactory::Create(SkeletonBaseView{skeleton_base}.GetAssociatedInstanceIdentifier(),
                                                   SkeletonBaseView{skeleton_base}.GetBinding(),
                                                   method_name,
                                                   method_type),
              method_type)
    {
        AssertMethodSignatureDoesNotContainPointersOrReferences<FailureMode::COMPILE_TIME, ReturnType, ArgTypes...>();
    }

    /// \brief Delegated constructor which registers the method with the parent skeleton.
    ///
    /// This constructor is only called directly by tests, since it allows for direct injection of a mock binding. In
    /// production code, the other constructors are used which create the binding internally using the factory.
    SkeletonMethod(SkeletonBase& skeleton_base,
                   const std::string_view method_name,
                   std::unique_ptr<SkeletonMethodBinding> skeleton_method_binding,
                   ::score::mw::com::impl::MethodType method_type = ::score::mw::com::impl::MethodType::kMethod);

    ~SkeletonMethod() = default;

    SkeletonMethod(const SkeletonMethod&) = delete;
    SkeletonMethod& operator=(const SkeletonMethod&) & = delete;

    SkeletonMethod(SkeletonMethod&& other) noexcept = default;
    SkeletonMethod& operator=(SkeletonMethod&& other) & noexcept = default;

    /// \brief Register a handler with the binding, which will be executed by the binding when the Proxy calls this
    /// method.
    /// \return score::cpp::blank on success and ComErrc code specified by the binding on failure
    template <typename Callable>
    Result<void> RegisterHandler(Callable&& callback);

    /// \brief Overload for handlers that accept a QualityType as their first argument.
    ///
    /// Used when SkeletonField Get support is enabled: RegisterGetHandler registers the handler through this overload
    /// so that the binding forwards the QualityType to it when invoked.
    /// \return score::cpp::blank on success and ComErrc code specified by the binding on failure
    template <typename Callable>
    Result<void> RegisterHandler(Callable&& callback, QualityType);

  private:
    template <typename Callable>
    Result<void> RegisterHandlerImpl(Callable&& callback);
};

template <typename ReturnType, typename... ArgTypes>
SkeletonMethod<ReturnType(ArgTypes...)>::SkeletonMethod(SkeletonBase& skeleton_base,
                                                        const std::string_view method_name,
                                                        std::unique_ptr<SkeletonMethodBinding> skeleton_method_binding,
                                                        ::score::mw::com::impl::MethodType method_type)
    : SkeletonMethodBase(method_name, std::move(skeleton_method_binding), method_type)
{
    SkeletonBaseView{skeleton_base}.RegisterMethod(method_name_, GetReferenceToMoveable());

    AssertMethodSignatureDoesNotContainPointersOrReferences<FailureMode::COMPILE_TIME, ReturnType, ArgTypes...>();
}

template <typename ReturnType, typename... ArgTypes>
template <typename Callable>
Result<void> SkeletonMethod<ReturnType(ArgTypes...)>::RegisterHandler(Callable&& callback)
{
    AssertMethodCallableIsNotStdBind<FailureMode::COMPILE_TIME, Callable>();
    AssertMethodHandlerSupportsMethodSignature<FailureMode::COMPILE_TIME, Callable, ReturnType, ArgTypes...>();

    // The user callback does not accept a QualityType. RegisterHandlerImpl detects this and invokes it without the
    // QualityType. std::forward preserves the lvalue/rvalue category so the ownership split happens once, in the impl.
    return RegisterHandlerImpl(std::forward<Callable>(callback));
}

template <typename ReturnType, typename... ArgTypes>
template <typename Callable>
Result<void> SkeletonMethod<ReturnType(ArgTypes...)>::RegisterHandler(Callable&& callback, QualityType)
{
    constexpr bool is_return_type_not_void = !std::is_same_v<ReturnType, void>;
    if constexpr (is_return_type_not_void)
    {
        static_assert(std::is_invocable_r_v<void, Callable, QualityType, ReturnType&, const ArgTypes&...>,
                      "Callable must have signature void(QualityType, ReturnType&, const ArgTypes&...)");
    }
    else
    {
        static_assert(std::is_invocable_r_v<void, Callable, QualityType, const ArgTypes&...>,
                      "Callable must have signature void(QualityType, const ArgTypes&...)");
    }
    // The callback already accepts a QualityType. RegisterHandlerImpl detects this and forwards the QualityType to it.
    return RegisterHandlerImpl(std::forward<Callable>(callback));
}

template <typename ReturnType, typename... ArgTypes>
template <typename Callable>
Result<void> SkeletonMethod<ReturnType(ArgTypes...)>::RegisterHandlerImpl(Callable&& callback)
{
    // If no binding was provided (e.g. in tests with incomplete mock setup), skip registration silently.
    if (binding_ == nullptr)
    {
        return Result<void>{};
    }

    // Since callback can be an lvalue reference or an rvalue reference, we ideally would store it as a universal
    // reference in the type_erased_handler. However, in C++17, this is not supported. Instead, we create a callable
    // here which will be called below by another lambda which explicitly stores the callback as either an lvalue
    // reference or an rvalue reference depending on how it was passed in.
    static auto stateless_type_erased_handler = [](Callable& actual_callback,
                                                   std::optional<score::cpp::span<std::byte>> type_erased_in_args,
                                                   std::optional<score::cpp::span<std::byte>> type_erased_return,
                                                   QualityType quality_type) {
        using InArgPtrTuple = std::tuple<ArgTypes*...>;
        InArgPtrTuple typed_in_arg_ptrs{};

        constexpr bool is_in_arg_pack_empty = (sizeof...(ArgTypes) == 0);

        if constexpr (!is_in_arg_pack_empty)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
                type_erased_in_args.has_value(),
                "ArgTypes is non void. Thus, type_erased_in_args needs to have a value!");
            typed_in_arg_ptrs = DeserializeArgs<ArgTypes...>(type_erased_in_args.value());
        }

        constexpr bool is_return_type_not_void_impl = !std::is_same_v<ReturnType, void>;
        if constexpr (is_return_type_not_void_impl)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
                type_erased_return.has_value(),
                "ReturnType is non void. Thus, type_erased_result needs to have a value!");
            auto* const typed_return_ptr = DeserializeArg<ReturnType>(type_erased_return.value());

            // Call the callable with the typed_return_ptr and the typed_in_arg_ptrs which are unpacked from the
            // tuple into individual arguments. The QualityType is only forwarded if the callable accepts it.
            std::apply(
                [&actual_callback, typed_return_ptr, quality_type](ArgTypes*... typed_in_arg_ptrs) {
                    detail::InvokeWithOptionalQuality(
                        quality_type, actual_callback, *typed_return_ptr, *typed_in_arg_ptrs...);
                },
                typed_in_arg_ptrs);
        }
        else
        {
            // Call the callable with the typed_in_arg_ptrs which are unpacked from the tuple into individual
            // arguments. The QualityType is only forwarded if the callable accepts it.
            std::apply(
                [&actual_callback, quality_type](ArgTypes*... typed_in_arg_ptrs) {
                    detail::InvokeWithOptionalQuality(quality_type, actual_callback, *typed_in_arg_ptrs...);
                },
                typed_in_arg_ptrs);
        }
    };

    if constexpr (std::is_lvalue_reference_v<Callable>)
    {
        SkeletonMethodBinding::TypeErasedHandler type_erased_handler =
            [&callback](std::optional<score::cpp::span<std::byte>> type_erased_in_args,
                        std::optional<score::cpp::span<std::byte>> type_erased_return,
                        QualityType quality_type) mutable {
                return stateless_type_erased_handler(callback, type_erased_in_args, type_erased_return, quality_type);
            };
        return binding_->RegisterHandler(std::move(type_erased_handler));
    }
    else
    {
        SkeletonMethodBinding::TypeErasedHandler type_erased_handler =
            [callback = std::move(callback)](std::optional<score::cpp::span<std::byte>> type_erased_in_args,
                                             std::optional<score::cpp::span<std::byte>> type_erased_return,
                                             QualityType quality_type) mutable {
                return stateless_type_erased_handler(callback, type_erased_in_args, type_erased_return, quality_type);
            };
        return binding_->RegisterHandler(std::move(type_erased_handler));
    }
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_H
