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

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_TEST_DATATYPE_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_TEST_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

constexpr const char* const kInstanceSpecifierString = "test/fields/set_and_notifier";

const std::int32_t kInitialValue = 18;
const std::int32_t kSetValidValue = 42;
const std::int32_t kSetRequestValue = 1234;
const std::int32_t kSetAcceptedValue = 100;

template <typename T>
class InitialOnlyInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t> test_field{*this, "test_field"};
};

template <typename T>
class SetEnabledInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, false, true> test_field{*this, "test_field"};
};

using InitialOnlyProxy = score::mw::com::AsProxy<InitialOnlyInterface>;
using InitialOnlySkeleton = score::mw::com::AsSkeleton<InitialOnlyInterface>;

using SetEnabledProxy = score::mw::com::AsProxy<SetEnabledInterface>;
using SetEnabledSkeleton = score::mw::com::AsSkeleton<SetEnabledInterface>;

// TODO: Add a getter-enabled field interface type for dedicated "get" mode integration scenarios.

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_TEST_DATATYPE_H
