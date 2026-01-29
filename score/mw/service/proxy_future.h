// *******************************************************************************
// Copyright (c) 2026 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

#ifndef SCORE_MW_SERVICE_PROXY_FUTURE_H
#define SCORE_MW_SERVICE_PROXY_FUTURE_H

#include <optional>

namespace score
{
namespace cpp
{
class stop_token;
}  // namespace cpp

namespace mw
{
namespace service
{

/**
 * @brief Future class for asynchronous proxy operations
 */
class ProxyFuture
{
  public:
    ProxyFuture() = default;
    virtual ~ProxyFuture() = default;
    ProxyFuture(ProxyFuture&&) = delete;
    ProxyFuture(const ProxyFuture&) = delete;

    ProxyFuture& operator=(ProxyFuture&&) = delete;
    ProxyFuture& operator=(const ProxyFuture&) = delete;

    /**
     * @brief Get the result of the future with stop token support
     * @tparam T The type of the proxy holder
     * @param stop_token Token to support cancellation
     * @return Optional containing the proxy holder if available
     */
    template <typename T = int>
    std::optional<T> Get(const score::cpp::stop_token& stop_token);
};

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_PROXY_FUTURE_H
