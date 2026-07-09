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
#ifndef SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_PROVIDER_RESTART_CONTROLLER_H
#define SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_PROVIDER_RESTART_CONTROLLER_H

#include <score/stop_token.hpp>

#include <string_view>

namespace score::mw::com::test
{

void DoProviderNormalRestartWithProxy(score::cpp::stop_token test_stop_token,
                                      std::string_view service_instance_manifest) noexcept;
void DoProviderNormalRestartWithoutProxy(score::cpp::stop_token test_stop_token,
                                         std::string_view service_instance_manifest) noexcept;
void DoProviderKillRestartWithProxy(score::cpp::stop_token test_stop_token,
                                    std::string_view service_instance_manifest) noexcept;
void DoProviderKillRestartWithoutProxy(score::cpp::stop_token test_stop_token,
                                       std::string_view service_instance_manifest) noexcept;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_PROVIDER_RESTART_CONTROLLER_H
