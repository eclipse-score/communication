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
#ifndef SCORE_MW_COM_TEST_CLOSE_ON_EXEC_TEST_PARAMETERS_H
#define SCORE_MW_COM_TEST_CLOSE_ON_EXEC_TEST_PARAMETERS_H

namespace score::mw::com::test
{

/// Argument of the test executable when it runs as the child that lists the descriptors it inherited.
inline constexpr auto kListFdsArgument{"--list-fds"};

/// Last line printed by that child. The parent requires it, so a child that never ran cannot pass the check.
inline constexpr auto kListFdsEndMarker{"end of descriptor list"};

/// Instance specifier of the service offered by every scenario (see config/mw_com_config.json).
inline constexpr auto kInstanceSpecifier{"test/close_on_exec/CloseOnExecInterface"};

/// Uid used in config/mw_com_config.json. The test replaces it with the uid it runs as.
inline constexpr auto kUidPlaceholder{"424242"};

/// The scenarios. Each one runs as its own test target (see BUILD).
inline constexpr auto kScenarioServiceOffered{"service_offered"};
inline constexpr auto kScenarioProxySubscribed{"proxy_subscribed"};
inline constexpr auto kScenarioServiceOfferedAgain{"service_offered_again"};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_CLOSE_ON_EXEC_TEST_PARAMETERS_H
