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
#ifndef SCORE_MW_COM_IMPL_MOCKING_I_PROXY_EVENT_H
#define SCORE_MW_COM_IMPL_MOCKING_I_PROXY_EVENT_H

#include "score/mw/com/impl/event_receive_handler.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/subscription_state.h"
#include "score/mw/com/impl/subscription_state_change_handler.h"

#include "score/result/result.h"

#include <score/callback.hpp>

#include <cstdint>

namespace score::mw::com::impl
{

class IProxyEventBase
{
  public:
    IProxyEventBase() = default;
    virtual ~IProxyEventBase() = default;

    virtual Result<void> Subscribe(const std::size_t) = 0;
    virtual void Unsubscribe() = 0;
    virtual SubscriptionState GetSubscriptionState() const = 0;
    virtual std::size_t GetFreeSampleCount() const = 0;
    virtual Result<std::size_t> GetNumNewSamplesAvailable() = 0;
    virtual Result<void> SetReceiveHandler(EventReceiveHandler) = 0;
    virtual Result<void> UnsetReceiveHandler() = 0;
    virtual Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) = 0;
    virtual Result<void> UnsetSubscriptionStateChangeHandler() = 0;

  protected:
    IProxyEventBase(const IProxyEventBase&) = default;
    IProxyEventBase(IProxyEventBase&&) noexcept = default;
    IProxyEventBase& operator=(IProxyEventBase&&) & noexcept = default;
    IProxyEventBase& operator=(const IProxyEventBase&) & = default;
};

template <typename SampleType>
class IProxyEvent : public IProxyEventBase
{
  public:
    IProxyEvent() = default;
    ~IProxyEvent() override = default;

    using Callback = score::cpp::callback<void(SamplePtr<SampleType>), 80U>;

    virtual Result<std::size_t> GetNewSamples(Callback&&, const std::size_t) = 0;

  protected:
    IProxyEvent(const IProxyEvent&) = default;
    IProxyEvent(IProxyEvent&&) noexcept = default;
    IProxyEvent& operator=(IProxyEvent&&) & noexcept = default;
    IProxyEvent& operator=(const IProxyEvent&) & = default;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_I_PROXY_EVENT_H
