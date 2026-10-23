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
#ifndef SCORE_MW_COM_TEST_METHODS_MULTIPLE_PROXIES_COMMON_RESOURCES_H
#define SCORE_MW_COM_TEST_METHODS_MULTIPLE_PROXIES_COMMON_RESOURCES_H

#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace score::mw::com::test
{

// Number of methods which exist within DuplicateSignatureInterface.
inline constexpr std::size_t kNumRegisteredMethods{5U};

inline constexpr std::int32_t kMethodResultMultiplierBase{13};

// The method index for each consumer which is enabled. E.g. the first vector contains the method indices enabled for
// consumer 0 etc. The size of this array must match num_consumers defined in the python test runner.
using EnabledMethodIdsPerConsumer = std::array<std::vector<std::size_t>, 4U>;
const EnabledMethodIdsPerConsumer kEnabledMethodsPerProxy{std::vector<std::size_t>{0U, 1U},
                                                          std::vector<std::size_t>{1U, 2U},
                                                          std::vector<std::size_t>{3U, 4U},
                                                          std::vector<std::size_t>{0U, 2U, 3U}};

std::string CreateInterprocessNotificationShmPath(size_t path_id);

/// \brief Utility function to print a line to the console in a thread safe manner (i.e. the entire string is printed at
/// once, without interleaving with other print statements from other threads).
template <typename... Args>
void PrintLine(Args&&... args)
{
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    std::cout << oss.str();
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_MULTIPLE_PROXIES_COMMON_RESOURCES_H
