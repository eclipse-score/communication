/*********************************************************************************
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

#ifndef SCORE_MW_SERVICE_GET_PROXY_H
#define SCORE_MW_SERVICE_GET_PROXY_H

#include "score/mw/service/proxy_future.h"

#include "score/mw/log/logging.h"

#include <chrono>
#include <string_view>
#include <tuple>
#include <utility>

namespace score::mw::service
{

template <typename ProxyType, typename ResolvedType = std::unique_ptr<ProxyType>>
auto GetProxyFromFuture(mw::service::ProxyFuture<ResolvedType>& proxy_future,
                        const std::string_view service_name,
                        const score::cpp::stop_token& token,
                        const std::chrono::milliseconds max_wait_time) -> ResolvedType
{
    ResolvedType result{};
    if (proxy_future.Valid())
    {
        if (proxy_future.WaitFor(token, max_wait_time).has_value())
        {
            auto proxy_holder = proxy_future.Get(token);
            if (proxy_holder.has_value())
            {
                result = std::move(proxy_holder).value();
                mw::log::LogInfo() << static_cast<const char*>(__func__) << service_name << "service was found";
            }
        }
    }

    return result;
}

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_GET_PROXY_H
