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
#ifndef SCORE_MW_COM_TEST_API_IDEMPOTENCY_API_IDEMPOTENCY_DATATYPE_H
#define SCORE_MW_COM_TEST_API_IDEMPOTENCY_API_IDEMPOTENCY_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>
#include <string_view>

namespace score::mw::com::test
{

using ApiIdempotencySample = std::int32_t;

template <typename Trait>
class ApiIdempotencyInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<ApiIdempotencySample> test_event{*this, "test_event"};
};

using ApiIdempotencyProxy = score::mw::com::AsProxy<ApiIdempotencyInterface>;
using ApiIdempotencySkeleton = score::mw::com::AsSkeleton<ApiIdempotencyInterface>;

constexpr std::string_view kApiIdempotencyInstanceSpecifierString =
    "/score/mw/com/test/api_idempotency/api_idempotency_instance";

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_API_IDEMPOTENCY_API_IDEMPOTENCY_DATATYPE_H
