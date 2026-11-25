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
#ifndef SCORE_MW_COM_TEST_GENERIC_PROXY_UNSUBSCRIBE_ON_DESTRUCTION_TEST_PARAMETERS_H
#define SCORE_MW_COM_TEST_GENERIC_PROXY_UNSUBSCRIBE_ON_DESTRUCTION_TEST_PARAMETERS_H

#include "score/mw/com/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace score::mw::com::test
{

const std::string kInterprocessNotificationShmPath{"/generic_proxy_unsub_on_destruction_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/generic_proxy/unsubscribe_on_destruction/TestEvents"}).value();

/// \brief Samples which the provider sends and the consumer expects to receive before destroying its GenericProxy.
///
/// The provider may send them before the consumer has subscribed. They are still received after subscribing, as long
/// as the number of samples does not exceed numberOfSampleSlots in config/mw_com_config.json.
const std::vector<std::uint32_t> kExpectedSamples{1U, 2U, 3U, 4U, 5U};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_GENERIC_PROXY_UNSUBSCRIBE_ON_DESTRUCTION_TEST_PARAMETERS_H
