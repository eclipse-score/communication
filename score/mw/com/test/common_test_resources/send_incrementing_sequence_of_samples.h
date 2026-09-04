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
#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SEND_INCREMENTING_SEQUENCE_OF_SAMPLES_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SEND_INCREMENTING_SEQUENCE_OF_SAMPLES_H

#include "score/mw/com/test/common_test_resources/fail_test.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>

namespace score::mw::com::test
{

/// Sends an incrementing sequence of number_of_samples_to_send_per_offer samples via
/// event, starting at initial_value.
template <typename Event>
void SendIncrementingSequenceOfSamples(Event& event,
                                       const std::size_t number_of_samples_to_send_per_offer,
                                       const std::uint32_t initial_value)
{
    std::cout << "\nProvider: Sending " << number_of_samples_to_send_per_offer << " samples" << std::endl;
    for (std::uint32_t i = 0; i < number_of_samples_to_send_per_offer; ++i)
    {
        auto send_result = event.Send(i + initial_value);
        if (!send_result.has_value())
        {
            FailTest("Provider: Send failed: ", send_result.error());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SEND_INCREMENTING_SEQUENCE_OF_SAMPLES_H
