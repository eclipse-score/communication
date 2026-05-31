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
#ifndef SCORE_MW_COM_IMPL_PROXY_FIELD_BASE_H
#define SCORE_MW_COM_IMPL_PROXY_FIELD_BASE_H

#include "score/mw/com/impl/proxy_event_base.h"

#include "score/result/result.h"

#include <score/assert.hpp>

#include <cstddef>
#include <functional>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

class ProxyBase;
class ProxyFieldBaseView;

class ProxyFieldBase
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a view to the private members of this class.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyFieldBaseView;

  public:
    ProxyFieldBase(ProxyBase& proxy_base, std::string_view field_name, ProxyEventBase* proxy_event_base_dispatch)
        : proxy_base_{proxy_base}, proxy_event_base_dispatch_{proxy_event_base_dispatch}, field_name_{field_name}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_event_base_dispatch != nullptr);
    }

    /// \brief A ProxyFieldBase shall not be copyable
    ProxyFieldBase(const ProxyFieldBase&) = delete;
    ProxyFieldBase& operator=(const ProxyFieldBase&) = delete;

    /// \brief A ProxyFieldBase shall be moveable
    ProxyFieldBase(ProxyFieldBase&&) noexcept = default;
    ProxyFieldBase& operator=(ProxyFieldBase&&) noexcept = default;

    virtual ~ProxyFieldBase() noexcept = default;

    void UpdateProxyReference(ProxyBase& proxy_base) noexcept
    {
        proxy_base_ = proxy_base;
    }

  protected:
    Result<void> Subscribe(const std::size_t max_sample_count) noexcept
    {
        return proxy_event_base_dispatch_->Subscribe(max_sample_count);
    }

    SubscriptionState GetSubscriptionState() noexcept
    {
        return proxy_event_base_dispatch_->GetSubscriptionState();
    }

    Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) noexcept
    {
        return proxy_event_base_dispatch_->SetSubscriptionStateChangeHandler(std::move(handler));
    }

    Result<void> UnsetSubscriptionStateChangeHandler() noexcept
    {
        return proxy_event_base_dispatch_->UnsetSubscriptionStateChangeHandler();
    }

    std::size_t GetFreeSampleCount() noexcept
    {
        return proxy_event_base_dispatch_->GetFreeSampleCount();
    }

    Result<std::size_t> GetNumNewSamplesAvailable() noexcept
    {
        return proxy_event_base_dispatch_->GetNumNewSamplesAvailable();
    }

    Result<void> SetReceiveHandler(EventReceiveHandler handler) noexcept
    {
        return proxy_event_base_dispatch_->SetReceiveHandler(std::move(handler));
    }

    Result<void> UnsetReceiveHandler() noexcept
    {
        return proxy_event_base_dispatch_->UnsetReceiveHandler();
    }

    void Unsubscribe() noexcept
    {
        // if the WithNotifier tag is not set, proxy_event_base_dispatch_ will be nullptr.
        if (proxy_event_base_dispatch_ != nullptr)
        {
            proxy_event_base_dispatch_->Unsubscribe();
        }
    }

    std::reference_wrapper<ProxyBase> proxy_base_;
    ProxyEventBase* proxy_event_base_dispatch_;
    std::string_view field_name_;
};

/// \brief View pattern to access ProxyFieldBase's private members and functions from ProxyBase without making them
/// public.
///
/// A method that must be visible to the impl namespace but hidden from the end user.
class ProxyFieldBaseView
{
  public:
    explicit ProxyFieldBaseView(ProxyFieldBase& base) noexcept : base_{base} {}

    void Unsubscribe() noexcept
    {
        base_.Unsubscribe();
    }

  private:
    ProxyFieldBase& base_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_FIELD_BASE_H
