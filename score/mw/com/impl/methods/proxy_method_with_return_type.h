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
#ifndef SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_WITH_RETURN_TYPE_H
#define SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_WITH_RETURN_TYPE_H

#include "score/mw/com/impl/methods/method_signature_element_ptr.h"
#include "score/mw/com/impl/methods/proxy_method.h"
#include "score/mw/com/impl/methods/proxy_method_base.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/stop_token.hpp>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Partial specialization of ProxyMethod for function signatures with no arguments and non-void return
/// \tparam ReturnType return type of the method
template <typename ReturnType>
class ProxyMethod<ReturnType()> final : public ProxyMethodBase
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
        : ProxyMethodBase(proxy_base, std::move(proxy_method_binding), method_name)
    {
    }

    ~ProxyMethod() final = default;

    /// \brief A ProxyMethod shall not be copyable.
    ProxyMethod(const ProxyMethod&) = delete;
    ProxyMethod& operator=(const ProxyMethod&) = delete;

    /// \brief A ProxyMethod shall be moveable.
    ProxyMethod(ProxyMethod&&) noexcept;
    ProxyMethod& operator=(ProxyMethod&&) noexcept;

    /// \brief This is the call-operator of ProxyMethod with no arguments for a non-void ReturnType.
    score::Result<MethodReturnTypePtr<ReturnType>> operator()();

  private:
    /// \brief Empty optional as in this class template specialization we do not have in-arguments.
    /// \details We still keep this member for interface consistency with the general ProxyMethod template
    /// specialization. The access via ProxyMethodView remains the same.
    static constexpr std::optional<memory::DataTypeSizeInfo> type_erased_in_args_{};

    /// \brief Compile-time initialized memory::DataTypeSizeInfo for the return type of this ProxyMethod.
    /// \details This is the only information about the return type of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    static constexpr std::optional<memory::DataTypeSizeInfo> type_erased_return_type_ =
        CreateDataTypeSizeInfoFromTypes<ReturnType>();
};

template <typename ReturnType>
ProxyMethod<ReturnType()>::ProxyMethod(ProxyMethod&& other) noexcept : ProxyMethodBase(std::move(other))
{
    // Since the address of this method has changed, we need update the address stored in the parent proxy.
    ProxyBaseView proxy_base_view{proxy_base_.get()};
    proxy_base_view.UpdateMethod(method_name_, *this);
}

template <typename ReturnType>
auto ProxyMethod<ReturnType()>::operator=(ProxyMethod&& other) noexcept -> ProxyMethod<ReturnType()>&
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

template <typename ReturnType>
score::Result<MethodReturnTypePtr<ReturnType>> ProxyMethod<ReturnType()>::operator()()
{
    auto queue_position_result = detail::DetermineNextAvailableQueueSlot(is_return_type_ptr_active_);
    if (!queue_position_result.has_value())
    {
        return Unexpected(queue_position_result.error());
    }

    const auto queue_position = queue_position_result.value();
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

#endif  // SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_WITH_RETURN_TYPE_H
