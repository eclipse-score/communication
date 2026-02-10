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

#include "score/concurrency/future/interruptible_future.h"

namespace score
{
namespace mw
{
namespace service
{

/**
 * @brief ProxyFuture is a stub/alias for InterruptibleFuture
 * @tparam T The type of the value held by the future
 */
template <typename T>
using ProxyFuture = score::concurrency::InterruptibleFuture<T>;

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_PROXY_FUTURE_H
