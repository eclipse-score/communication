/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_EVENT_H

#include "score/mw/com/impl/bindings/someip/element_fq_id.h"
#include "score/mw/com/impl/proxy_event_binding.h"

#include <string_view>

namespace score::mw::com::impl::someip
{

/// \brief Proxy event binding implementation for the SOME/IP binding.
class ProxyEvent final : public ProxyEventBinding
{
  public:
    ProxyEvent(const ElementFqId element_fq_id, const std::string_view event_name) noexcept;

    Result<void> Subscribe(std::size_t max_sample_count) noexcept override;
    SubscriptionState GetSubscriptionState() const noexcept override;
    void Unsubscribe() noexcept override;
    Result<void> SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept override;
    Result<void> UnsetReceiveHandler() noexcept override;
    Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) noexcept override;
    Result<void> UnsetSubscriptionStateChangeHandler() noexcept override;
    Result<std::size_t> GetNumNewSamplesAvailable() const override;
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept override;
    BindingType GetBindingType() const noexcept override;
    Result<std::size_t> GetNewSamples(Callback&& receiver, TrackerGuardFactory& tracker) override;

    ElementFqId GetElementFQId() const noexcept
    {
        return element_fq_id_;
    }

  private:
    ElementFqId element_fq_id_;
    std::string_view event_name_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_EVENT_H