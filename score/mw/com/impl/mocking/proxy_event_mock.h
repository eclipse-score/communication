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
#ifndef SCORE_MW_COM_IMPL_MOCKING_PROXY_EVENT_MOCK_H
#define SCORE_MW_COM_IMPL_MOCKING_PROXY_EVENT_MOCK_H

#include "score/mw/com/impl/mocking/i_proxy_event.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl
{

template <typename SampleType>
class ProxyEventMock : public IProxyEvent<SampleType>
{
  public:
    using Callback = typename IProxyEvent<SampleType>::Callback;

    MOCK_METHOD(Result<void>, Subscribe, (const std::size_t), (override));
    MOCK_METHOD(void, Unsubscribe, (), (override));
    MOCK_METHOD(SubscriptionState, GetSubscriptionState, (), (const, override));
    MOCK_METHOD(std::size_t, GetFreeSampleCount, (), (const, override));
    MOCK_METHOD(Result<std::size_t>, GetNumNewSamplesAvailable, (), (override));
    MOCK_METHOD(Result<void>, SetReceiveHandler, (EventReceiveHandler), (override));
    MOCK_METHOD(Result<void>, UnsetReceiveHandler, (), (override));

    MOCK_METHOD(Result<std::size_t>, GetNewSamples, (Callback&&, const std::size_t), (override));
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_PROXY_EVENT_MOCK_H
