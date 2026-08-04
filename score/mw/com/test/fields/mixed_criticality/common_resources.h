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
#ifndef SCORE_MW_COM_TEST_FIELDS_MIXED_CRITICALITY_COMMON_RESOURCES_H
#define SCORE_MW_COM_TEST_FIELDS_MIXED_CRITICALITY_COMMON_RESOURCES_H

#include "score/mw/com/types.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace score::mw::com::test
{

std::string ParseServiceInstanceManifest(int argc, const char** argv);

const std::string kInterprocessNotificationShmPath{"/fields_mixed_criticality_test_interprocess_notification"};
const std::string kFailureMessagePrefix{"fields_mixed_criticality"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/fields/mixed_criticality/FieldSignature"}).value();

constexpr std::int32_t kInitialValue{100};
const std::vector<std::int32_t> kFollowupValues{101, 102, 103, 104};
const std::vector<std::int32_t> kExpectedFieldSamples{kInitialValue, 101, 102, 103, 104};

constexpr std::int32_t kSetRequestValue{11};
constexpr std::int32_t kExpectedAcceptedSetValue{(kSetRequestValue * 2) + 1};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_MIXED_CRITICALITY_COMMON_RESOURCES_H
