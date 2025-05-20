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

#ifndef PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_PROXY_RESTART_SHALL_NOT_AFFECT_OTHER_PROXIES_PROVIDER_H
#define PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_PROXY_RESTART_SHALL_NOT_AFFECT_OTHER_PROXIES_PROVIDER_H
#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

namespace bmw::mw::com::test
{
    void PerformProviderActions(CheckPointControl &check_point_control, amp::stop_token stop_token);
}
#endif // PLATFORM_AAS_MW_COM_TEST_PARTIAL_RESTART_PROXY_RESTART_SHALL_NOT_AFFECT_OTHER_PROXIES_PROVIDER_H
