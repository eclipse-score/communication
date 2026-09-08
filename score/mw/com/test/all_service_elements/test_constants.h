/*******************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_ALL_SERVICE_ELEMENTS_TEST_CONSTANTS_H
#define SCORE_MW_COM_TEST_ALL_SERVICE_ELEMENTS_TEST_CONSTANTS_H

#include "score/mw/com/test/all_service_elements/all_service_elements_datatype.h"
#include "score/mw/com/types.h"

#include <string>
#include <vector>

namespace score::mw::com::test
{

const std::string kInterprocessNotificationShmPath{"/all_service_elements_test_interprocess_notification"};
const std::string kFailureMessagePrefix{"all_service_elements"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"/score/mw/com/test/all_service_elements/all_service_elements_instance"})
        .value();

// Method test values
const TestType kReturnOnlyMethodReturnValue{15};
const TestType kInArgOnlyMethodTestValueA{17};
const TestType kInArgOnlyMethodTestValueB{18};
const TestType kInArgsAndReturnMethodTestValueA{42};
const TestType kInArgsAndReturnMethodTestValueB{23};

// Event test values
const std::vector<TestType> kEvent1ValuesToSend{21U, 22U, 23U};
const std::vector<TestType> kEvent2ValuesToSend{100U, 200U, 300U};

// Field initial values, set by the provider before offering the service
const TestType kGetOnlyFieldInitialValue{1};
const TestType kSetAndGetFieldInitialValue{2};
const TestType kGetAndNotifierFieldInitialValue{3};
const TestType kSetAndNotifierFieldInitialValue{4};
const TestType kSetAndGetAndNotifierFieldInitialValue{5};
const TestType kNotifierOnlyFieldInitialValue{6};

// Field values to send, set by the provider after offering the service
const std::vector<TestType> kGetAndNotifierFieldValuesToSend{7, 8, 9};
const std::vector<TestType> kSetAndNotifierFieldValuesToSend{10, 11, 12};
const std::vector<TestType> kSetAndGetAndNotifierFieldValuesToSend{13, 14, 15};
const std::vector<TestType> kNotifierOnlyFieldValuesToSend{16, 17, 18};

const std::vector<TestType> kAllGetAndNotifierValues{kGetAndNotifierFieldInitialValue, 7, 8, 9};
const std::vector<TestType> kAllSetAndNotifierValues{kSetAndNotifierFieldInitialValue, 10, 11, 12};
const std::vector<TestType> kAllSetAndGetAndNotifierValues{kSetAndGetAndNotifierFieldInitialValue, 13, 14, 15};
const std::vector<TestType> kAllNotifierOnlyValues{kNotifierOnlyFieldInitialValue, 16, 17, 18};

// Value requested by the consumer via Set()
const TestType kSetAndGetRequestValue{51};
const TestType kSetAndNotifierRequestValue{50};
const TestType kSetAndGetAndNotifierRequestValue{52};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_ALL_SERVICE_ELEMENTS_TEST_CONSTANTS_H
