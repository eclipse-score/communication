/*********************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_SERVICE_FIND_SERVICE_STRATEGY_MOCK_H
#define SCORE_MW_SERVICE_FIND_SERVICE_STRATEGY_MOCK_H

#include "score/mw/service/find_service_strategy.h"

#include <gmock/gmock.h>

#include <memory>
#include <utility>

namespace score::mw::service
{

template <typename ProxyType>
class FindServiceStrategyMock : public FindServiceStrategy<ProxyType>
{
  public:
    using typename FindServiceStrategy<ProxyType>::OnServiceFoundCallback;

    MOCK_METHOD(void, StartFind, (std::unique_ptr<OnServiceFoundCallback>), (override));
    MOCK_METHOD(void, StopFind, (), (noexcept, override));
};

template <typename ProxyType>
class FindServiceStrategyMockFacade : public FindServiceStrategy<ProxyType>
{
  public:
    using typename FindServiceStrategy<ProxyType>::OnServiceFoundCallback;

    FindServiceStrategyMockFacade(FindServiceStrategy<ProxyType>& find_service_strategy)
        : find_service_strategy_{find_service_strategy}
    {
    }

    ~FindServiceStrategyMockFacade() override = default;

    void StartFind(std::unique_ptr<OnServiceFoundCallback> on_found) override
    {
        find_service_strategy_.StartFind(std::move(on_found));
    }

    void StopFind() noexcept override
    {
        find_service_strategy_.StopFind();
    }

  private:
    FindServiceStrategy<ProxyType>& find_service_strategy_;
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_FIND_SERVICE_STRATEGY_MOCK_H
