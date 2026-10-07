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
#include "score/mw/com/impl/bindings/someip/proxy_event.h"

#include "score/mw/com/impl/com_error.h"

namespace score::mw::com::impl::someip
{

ProxyEvent::ProxyEvent(const ElementFqId element_fq_id, const std::string_view event_name) noexcept
    : ProxyEventBinding{}, element_fq_id_{element_fq_id}, event_name_{event_name}
{
}

Result<void> ProxyEvent::Subscribe(const std::size_t) noexcept
{
    return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP event subscription is unsupported without a transport");
}

SubscriptionState ProxyEvent::GetSubscriptionState() const noexcept
{
    return SubscriptionState::kNotSubscribed;
}

void ProxyEvent::Unsubscribe() noexcept {}

Result<void> ProxyEvent::SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler>) noexcept
{
    return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP event reception is unsupported without a transport");
}

Result<void> ProxyEvent::UnsetReceiveHandler() noexcept
{
    return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP event reception is unsupported without a transport");
}

Result<void> ProxyEvent::SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler) noexcept
{
    return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP event subscription is unsupported without a transport");
}

Result<void> ProxyEvent::UnsetSubscriptionStateChangeHandler() noexcept
{
    return MakeUnexpected(ComErrc::kBindingFailure, "SOME/IP event subscription is unsupported without a transport");
}

Result<std::size_t> ProxyEvent::GetNumNewSamplesAvailable() const
{
    return MakeUnexpected(ComErrc::kNotSubscribed);
}

std::optional<std::uint16_t> ProxyEvent::GetMaxSampleCount() const noexcept
{
    return std::nullopt;
}

BindingType ProxyEvent::GetBindingType() const noexcept
{
    return BindingType::kSomeIp;
}

Result<std::size_t> ProxyEvent::GetNewSamples(Callback&&, TrackerGuardFactory&)
{
    return MakeUnexpected(ComErrc::kNotSubscribed);
}

}  // namespace score::mw::com::impl::someip