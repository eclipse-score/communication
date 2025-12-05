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
#ifndef SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_WITH_IN_ARGS_AND_RETURN_H
#define SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_WITH_IN_ARGS_AND_RETURN_H

#include "score/mw/com/impl/methods/method_signature_element_ptr.h"
#include "score/mw/com/impl/methods/proxy_method.h"
#include "score/mw/com/impl/methods/proxy_method_base.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include "score/containers/dynamic_array.h"
#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/stop_token.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Partial specialization of ProxyMethod for function signatures with arguments and non-void return
/// \tparam ReturnType return type of the method
/// \tparam ArgTypes argument types of the method
template <typename ReturnType, typename... ArgTypes>
class ProxyMethod<ReturnType(ArgTypes...)> final : public ProxyMethodBase
{
    template <typename R, typename... A>
    // Design decision: This friend class provides a view on the internals of ProxyMethod.
    // This enables us to hide unnecessary internals from the end-user.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyMethodView;

  public:
    ProxyMethod(ProxyBase& proxy_base,
                std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                std::string_view method_name) noexcept
        : ProxyMethodBase(proxy_base, std::move(proxy_method_binding), method_name),
          are_in_arg_ptrs_active_{kCallQueueSize}
    {
    }

    ~ProxyMethod() final = default;

    /// \brief A ProxyMethod shall not be copyable.
    ProxyMethod(const ProxyMethod&) = delete;
    ProxyMethod& operator=(const ProxyMethod&) = delete;

    /// \brief A ProxyMethod shall be moveable.
    ProxyMethod(ProxyMethod&&) noexcept;
    ProxyMethod& operator=(ProxyMethod&&) noexcept;

    /// \brief Allocates the necessary storage for the argument values and the return value of a method call.
    /// \return On success, a tuple of MethodInArgPtr for each argument type is returned. On failure, an error code is
    /// returned.
    score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>> Allocate()
    {
        return detail::AllocateImpl<ArgTypes...>(*binding_, are_in_arg_ptrs_active_, is_return_type_ptr_active_);
    };

    /// \brief This is the copying call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as references. It then internally calls Allocate()
    /// to get the needed storage for the arguments and return value and copies the arguments into the allocated
    /// storage.
    score::Result<MethodReturnTypePtr<ReturnType>> operator()(const ArgTypes&... args);

    /// \brief This is the zero-copy call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as MethodInArgPtr, i.e. as pointers to the
    /// argument values, which have been allocated before via Allocate() call.
    score::Result<MethodReturnTypePtr<ReturnType>> operator()(MethodInArgPtr<ArgTypes>... args);

  private:
    /// \brief Compile-time initialized memory::DataTypeSizeInfo for the argument types of this ProxyMethod.
    /// \details This is the only information about the argument types of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    /// \remark If there are no arguments, this is std::nullopt.
    static constexpr std::optional<memory::DataTypeSizeInfo> type_erased_in_args_ =
        CreateDataTypeSizeInfoFromTypes<ArgTypes...>();

    /// \brief Compile-time initialized memory::DataTypeSizeInfo for the return type of this ProxyMethod.
    /// \details This is the only information about the return type of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    static constexpr std::optional<memory::DataTypeSizeInfo> type_erased_return_type_ =
        CreateDataTypeSizeInfoFromTypes<ReturnType>();

    /// \brief Outer dynamic array: one entry per call-queue position, inner array: one entry per argument.
    /// \details This array of arrays contains bool flags, which indicate, if the corresponding argument pointer
    /// passed to the zero-copy call-operator is active (true) or not (false).
    /// E.g. are_in_arg_ptrs_active_[0][2] == true means, that for the call-queue position 0, the 3rd argument
    /// pointer passed to the zero-copy call-operator is active.
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>> are_in_arg_ptrs_active_;
};

template <typename ReturnType, typename... ArgTypes>
ProxyMethod<ReturnType(ArgTypes...)>::ProxyMethod(ProxyMethod&& other) noexcept : ProxyMethodBase(std::move(other))
{
    // Since the address of this method has changed, we need update the address stored in the parent proxy.
    ProxyBaseView proxy_base_view{proxy_base_.get()};
    proxy_base_view.UpdateMethod(method_name_, *this);
}

template <typename ReturnType, typename... ArgTypes>
auto ProxyMethod<ReturnType(ArgTypes...)>::operator=(ProxyMethod&& other) noexcept
    -> ProxyMethod<ReturnType(ArgTypes...)>&
{
    if (this != &other)
    {
        ProxyMethod::operator=(std::move(other));

        // Since the address of this method has changed, we need update the address stored in the parent proxy.
        ProxyBaseView proxy_base_view{proxy_base_.get()};
        proxy_base_view.UpdateMethod(method_name_, *this);
    }
    return *this;
}

template <typename ReturnType, typename... ArgTypes>
score::Result<MethodReturnTypePtr<ReturnType>> ProxyMethod<ReturnType(ArgTypes...)>::operator()(const ArgTypes&... args)
{
    auto allocate_result = Allocate();
    if (!allocate_result.has_value())
    {
        return Unexpected(allocate_result.error());
    }
    auto& in_arg_ptr_tuple = allocate_result.value();

    // now copy the argument values into the allocated storage and call the other operator() taking MethodInArgPtr
    return std::apply(
        [this, &args...](auto&&... in_args_ptrs) {
            ((*(in_args_ptrs.get()) = args), ...);
            // attention: the explicit "this->" here was needed to make gcc8 happy! Without it, it crashed.
            return this->operator()(std::move(in_args_ptrs)...);
        },
        in_arg_ptr_tuple);
}

template <typename ReturnType, typename... ArgTypes>
score::Result<MethodReturnTypePtr<ReturnType>> ProxyMethod<ReturnType(ArgTypes...)>::operator()(
    MethodInArgPtr<ArgTypes>... args)
{
    auto queue_position = detail::GetCommonQueuePosition(args...);
    auto allocated_return_type_storage = binding_->AllocateReturnType(queue_position);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(allocated_return_type_storage.has_value(),
                           "ProxyMethod::operator(): AllocateReturnType failed unexpectedly.");
    auto call_result = binding_->DoCall(queue_position);
    if (!call_result.has_value())
    {
        return Unexpected(call_result.error());
    }

    return MethodReturnTypePtr<ReturnType>{
        *(reinterpret_cast<ReturnType*>(allocated_return_type_storage.value().data())),
        is_return_type_ptr_active_[queue_position],
        queue_position};
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_WITH_IN_ARGS_AND_RETURN_H
