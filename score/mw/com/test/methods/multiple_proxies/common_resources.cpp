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
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"

#include <string>

namespace score::mw::com::test
{

std::string CreateInterprocessNotificationShmPath(size_t path_id)
{
    std::string path{"test_methods_interprocess_shm_notification_path"};
    path.append(std::to_string(path_id));
    path.append("_notification");
    return path;
}

/// \brief Calculate the total number of times each method is expected to be called, based on the number of calls per
/// method, the number of proxies and the enabled methods for each consumer.
/// \return A vector where the value at each index corresponds to the expected number of calls for the corresponding
/// method in DuplicateSignatureInterface.
std::vector<std::size_t> CalculateExpectedMethodCallCounts(std::size_t num_consumers,
                                                           std::size_t num_proxies_per_process,
                                                           std::size_t num_method_calls_per_proxy,
                                                           const EnabledMethodIdsPerConsumer& enabled_method_ids)
{
    std::vector<std::size_t> expected_method_call_count(kNumRegisteredMethods, 0U);

    for (std::size_t consumer_id{0}; consumer_id < num_consumers; ++consumer_id)
    {
        const auto enabled_methods_for_consumer = enabled_method_ids.at(consumer_id);
        for (const auto method_id : enabled_methods_for_consumer)
        {
            expected_method_call_count.at(method_id) += num_proxies_per_process * num_method_calls_per_proxy;
        }
    }
    return expected_method_call_count;
}

}  // namespace score::mw::com::test
