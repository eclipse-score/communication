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

#ifndef SCORE_MW_COM_TEST_FIELDS_TEST_RESOURCES_GETTER_AND_SETTER_CHECKERS_H
#define SCORE_MW_COM_TEST_FIELDS_TEST_RESOURCES_GETTER_AND_SETTER_CHECKERS_H

#include "score/mw/com/test/common_test_resources/fail_test.h"

#include <score/stop_token.hpp>

#include <cstdint>
#include <iostream>

namespace score::mw::com::test
{

template <typename ProxyFieldType>
void CallGetAndCheckValue(ProxyFieldType& proxy_field, const std::int32_t expected_value)
{
    const auto get_result = proxy_field.Get();
    if (!get_result.has_value())
    {
        FailTest("Consumer: Get() failed: ", get_result.error());
    }
    if (*(get_result.value()) != expected_value)
    {
        FailTest("Consumer: Get() returned ", *(get_result.value()), " but expected ", expected_value);
    }

    std::cout << "\nConsumer: Get() returned expected value " << expected_value << std::endl;
}

template <typename ProxyFieldType>
void CallSetAndCheckReturnValue(ProxyFieldType& proxy_field,
                                const std::int32_t set_request_value,
                                const std::int32_t expected_accepted_value)
{
    const auto set_result = proxy_field.Set(set_request_value);
    if (!set_result.has_value())
    {
        FailTest("Consumer: Set() failed: ", set_result.error());
    }

    const std::int32_t accepted_value = *(set_result.value());
    if (accepted_value != expected_accepted_value)
    {
        FailTest("Consumer: Set() returned accepted value ", accepted_value, " but expected ", expected_accepted_value);
    }

    std::cout << "\nConsumer: Set() returned expected accepted value " << accepted_value << std::endl;
}

template <typename ProxyFieldType>
void CallGetUntilExpectedValueReceived(ProxyFieldType& proxy_field,
                                       const std::int32_t latest_expected_value,
                                       const score::cpp::stop_token& stop_token)
{
    while (!stop_token.stop_requested())
    {
        const auto get_result = proxy_field.Get();
        if (!get_result.has_value())
        {
            FailTest("Consumer: Get() failed: ", get_result.error());
        }
        if (*(get_result.value()) == latest_expected_value)
        {
            std::cout << "\nConsumer: Get() returned expected updated value " << latest_expected_value << std::endl;
            break;
        }
    }
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_TEST_RESOURCES_GETTER_AND_SETTER_CHECKERS_H
