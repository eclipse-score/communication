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
#ifndef SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_WITHOUT_IN_ARGS_OR_RETURN_H
#define SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_WITHOUT_IN_ARGS_OR_RETURN_H

#include "score/mw/com/impl/methods/proxy_method.h"
#include "score/mw/com/impl/methods/proxy_method_base.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory.h"
#include "score/mw/com/impl/proxy_base.h"

#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <score/stop_token.hpp>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Partial specialization of ProxyMethod for function signatures with no arguments and void return.
template <>
class ProxyMethod<void()> final : public ProxyMethodBase
{
    template <typename R, typename... A>
    // Design decision: This friend class provides a view on the internals of ProxyMethod.
    // This enables us to hide unnecessary internals from the end-user.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyMethodView;

  public:
    ProxyMethod(ProxyBase& proxy_base, std::string_view method_name) noexcept
        : ProxyMethodBase(proxy_base,
                          ProxyMethodBindingFactory<void()>::Create(proxy_base.GetHandle(),
                                                                    ProxyBaseView{proxy_base}.GetBinding(),
                                                                    method_name),
                          method_name)
    {
        auto proxy_base_view = ProxyBaseView{proxy_base};
        proxy_base_view.RegisterMethod(method_name_, *this);
        if (binding_ == nullptr)
        {
            proxy_base_view.MarkServiceElementBindingInvalid();
            return;
        }
    }

    ProxyMethod(ProxyBase& proxy_base,
                std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                std::string_view method_name) noexcept
        : ProxyMethodBase(proxy_base, std::move(proxy_method_binding), method_name)
    {
        auto proxy_base_view = ProxyBaseView{proxy_base};
        proxy_base_view.RegisterMethod(method_name_, *this);
        if (binding_ == nullptr)
        {
            proxy_base_view.MarkServiceElementBindingInvalid();
            return;
        }
    }

    ~ProxyMethod() final = default;

    /// \brief A ProxyMethod shall not be copyable.
    ProxyMethod(const ProxyMethod&) = delete;
    ProxyMethod& operator=(const ProxyMethod&) = delete;

    /// \brief A ProxyMethod shall be moveable.
    ProxyMethod(ProxyMethod&&) noexcept;
    ProxyMethod& operator=(ProxyMethod&&) noexcept;

    /// \brief This is the call-operator of ProxyMethod with no arguments and a void ReturnType.
    score::ResultBlank operator()();

  private:
    /// \brief Empty optional as in this class template specialization we do not have in-arguments.
    /// \details We still keep this member for interface consistency with the general ProxyMethod template
    /// specialization. The access via ProxyMethodView remains the same.
    static constexpr std::optional<memory::DataTypeSizeInfo> type_erased_in_args_{};

    /// \brief Empty optional as in this class template specialization we do not have a return type.
    /// \details We still keep this member for interface consistency with the general ProxyMethod template
    /// specialization. The access via ProxyMethodView remains the same.
    static constexpr std::optional<memory::DataTypeSizeInfo> type_erased_return_type_{};
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_WITHOUT_IN_ARGS_OR_RETURN_H
