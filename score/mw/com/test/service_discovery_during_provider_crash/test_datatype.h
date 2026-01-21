/*******************************************************************************
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

#ifndef SCORE_MW_COM_TEST_SERVICE_DISCOVERY_DURING_PROVIDER_CRASH_TEST_DATATYPE_H
#define SCORE_MW_COM_TEST_SERVICE_DISCOVERY_DURING_PROVIDER_CRASH_TEST_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>
#include <vector>

namespace score::mw::com::test
{

struct SimpleEventDatatype
{
    std::uint32_t member_1;
    std::uint32_t member_2;
};

template <typename Trait>
class TestServiceInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    static const std::vector<const char*> event_names;

    typename Trait::template Event<SimpleEventDatatype> simple_event_{*this, event_names[0]};
};
template <typename Trait>
const std::vector<const char*> TestServiceInterface<Trait>::event_names{"simple_event"};

using TestServiceProxy = AsProxy<TestServiceInterface>;
using TestServiceSkeleton = AsSkeleton<TestServiceInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_SERVICE_DISCOVERY_DURING_PROVIDER_CRASH_TEST_DATATYPE_H
