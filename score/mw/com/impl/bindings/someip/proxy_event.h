/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_EVENT_H

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/proxy_event_binding.h"

namespace score::mw::com::impl::someip
{

template <typename SampleType>
class ProxyEvent final : public ProxyEventBinding<SampleType>
{
  public:
    using typename ProxyEventBinding<SampleType>::Callback;

    Result<void> Subscribe(std::size_t) noexcept override { return MakeUnexpected(ComErrc::kBindingFailure); }
    SubscriptionState GetSubscriptionState() const noexcept override { return SubscriptionState::kNotSubscribed; }
    void Unsubscribe() noexcept override {}
    Result<void> SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler>) noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<void> UnsetReceiveHandler() noexcept override { return MakeUnexpected(ComErrc::kBindingFailure); }
    Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler) noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<void> UnsetSubscriptionStateChangeHandler() noexcept override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<std::size_t> GetNumNewSamplesAvailable() const override { return std::size_t{0}; }
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept override { return {}; }
    BindingType GetBindingType() const noexcept override { return BindingType::kSomeIp; }
    void NotifyServiceInstanceChangedAvailability(bool, pid_t) noexcept override {}
    Result<std::size_t> GetNewSamples(Callback&&, TrackerGuardFactory&) noexcept override
    {
        return std::size_t{0};
    }
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_EVENT_H
