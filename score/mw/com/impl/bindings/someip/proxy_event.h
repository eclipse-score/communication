/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_EVENT_H

#include "score/mw/com/impl/bindings/someip/proxy.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/mw/com/impl/proxy_event_binding.h"

#include <string>
#include <string_view>

namespace score::mw::com::impl::someip
{

/// \brief Proxy side event binding of the SOME/IP technical binding.
///
/// Created by the binding independent ProxyEventBindingFactoryImpl, which resolves the wire event id from the SOME/IP
/// service type deployment and passes the already created someip::Proxy as parent.
template <typename SampleType>
class ProxyEvent final : public ProxyEventBinding<SampleType>
{
  public:
    using typename ProxyEventBinding<SampleType>::Callback;

    ProxyEvent(Proxy& parent, const SomeIpEventId event_id, const std::string_view event_name) noexcept
        : ProxyEventBinding<SampleType>{}, parent_{parent}, event_id_{event_id}, event_name_{event_name}
    {
    }

    const Proxy& GetParent() const noexcept
    {
        return parent_;
    }

    SomeIpEventId GetEventId() const noexcept
    {
        return event_id_;
    }

    std::string_view GetEventName() const noexcept
    {
        return event_name_;
    }

    Result<void> Subscribe(std::size_t) noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    SubscriptionState GetSubscriptionState() const noexcept override
    {
        return SubscriptionState::kNotSubscribed;
    }
    void Unsubscribe() noexcept override {}
    Result<void> SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler>) noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<void> UnsetReceiveHandler() noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler) noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<void> UnsetSubscriptionStateChangeHandler() noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<std::size_t> GetNumNewSamplesAvailable() const override
    {
        return std::size_t{0};
    }
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept override
    {
        return {};
    }
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kSomeIp;
    }
    void NotifyServiceInstanceChangedAvailability(bool, pid_t) noexcept override {}
    Result<std::size_t> GetNewSamples(Callback&&, TrackerGuardFactory&) noexcept override
    {
        return std::size_t{0};
    }

  private:
    Proxy& parent_;
    SomeIpEventId event_id_;
    std::string event_name_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_EVENT_H
