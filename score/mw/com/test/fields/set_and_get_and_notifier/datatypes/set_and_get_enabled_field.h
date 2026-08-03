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

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_GET_AND_NOTIFIER_SET_AND_GET_ENABLED_FIELD_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_GET_AND_NOTIFIER_SET_AND_GET_ENABLED_FIELD_H

#include "score/mw/com/types.h"
#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class SetAndGetEnabledInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, WithSetter, WithGetter> set_and_get_enabled_field{
        *this,
        "set_and_get_enabled_field"};
};

using SetAndGetEnabledProxy = score::mw::com::AsProxy<SetAndGetEnabledInterface>;
using SetAndGetEnabledSkeleton = score::mw::com::AsSkeleton<SetAndGetEnabledInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_GET_AND_NOTIFIER_SET_AND_GET_ENABLED_FIELD_H
