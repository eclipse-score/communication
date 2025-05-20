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

#ifndef PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_PROVIDER_RESTART_MAX_SUBSCRIBERS_CONSUMER_H
#define PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_PROVIDER_RESTART_MAX_SUBSCRIBERS_CONSUMER_H

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"

#include <vector>

namespace bmw::mw::com::test
{

  struct ConsumerParameters
  {
    bool is_proxy_connected_during_restart;
    std::size_t max_number_subscribers;
  };

  class ConsumerActions
  {
  public:
    ConsumerActions(CheckPointControl &check_point_control,
                    amp::stop_token test_stop_token,
                    int argc,
                    const char **argv,
                    ConsumerParameters consumer_parameters) noexcept;
    void DoConsumerActions() noexcept;

  private:
    void DoConsumerActionsBeforeRestart() noexcept;
    void DoConsumerActionsAfterRestart() noexcept;

    CheckPointControl &check_point_control_;
    amp::stop_token test_stop_token_;
    ConsumerParameters consumer_parameters_;
    std::vector<TestServiceProxy> proxies_;
  };

} // namespace bmw::mw::com::test

#endif // PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_PROVIDER_RESTART_MAX_SUBSCRIBERS_CONSUMER_H
