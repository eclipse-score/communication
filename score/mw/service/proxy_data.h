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

#ifndef SCORE_MW_SERVICE_PROXY_DATA_H
#define SCORE_MW_SERVICE_PROXY_DATA_H

#include "score/mw/service/proxy_needs.h"

namespace score
{
namespace mw
{
namespace service
{

/// @brief Stub alias for OptionalProxyData - wraps an optional proxy instance.
/// @tparam T The proxy interface type
template <typename T>
using OptionalProxyData = Optional<T>;

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_PROXY_DATA_H
