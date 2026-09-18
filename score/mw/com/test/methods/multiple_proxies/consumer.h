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
#ifndef SCORE_MW_COM_TEST_METHODS_MULTIPLE_PROXIES_CONSUMER_H
#define SCORE_MW_COM_TEST_METHODS_MULTIPLE_PROXIES_CONSUMER_H

#include <cstddef>
#include <vector>

namespace score::mw::com::test
{

void run_consumer(const std::size_t consumer_id,
                  const std::size_t num_proxies_per_process,
                  const std::size_t num_method_calls_per_proxy,
                  const std::vector<std::size_t>& enabled_method_ids);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_MULTIPLE_PROXIES_CONSUMER_H
