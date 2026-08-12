/*******************************************************************************
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

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_GET_AND_NOTIFIER_TEST_CONSTANTS_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_GET_AND_NOTIFIER_TEST_CONSTANTS_H

#include "score/mw/com/types.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace score::mw::com::test
{

constexpr const char* const kInstanceSpecifierString = "/score/mw/com/test/fields/set_and_get_and_notifier_instance";
const std::string kConsumerDoneShmPath{"/fields_set_and_get_and_notifier_consumer_done"};
const std::string kSetDoneShmPath{"/fields_set_and_get_and_notifier_set_done"};
const std::string kConsumerGotValueShmPath = "/fields_set_and_get_and_notifier_consumer_got_value";
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{kInstanceSpecifierString}).value();

constexpr std::int32_t kInitialValue = 18;
constexpr std::int32_t kSetRequestValue = 1234;
constexpr std::int32_t kUpdatedValue = 19;

constexpr std::size_t kTotalNumValuesToSend = 4U;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_GET_AND_NOTIFIER_TEST_CONSTANTS_H
