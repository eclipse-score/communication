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

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_GETTER_ONLY_ENABLED_FIELD_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_GETTER_ONLY_ENABLED_FIELD_H

#include "score/mw/com/types.h"
#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class GetterOnlyEnabledInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, WithGetter> getter_only_enabled_field{*this, "get_only_enabled_field"};
};

using GetterOnlyEnabledProxy = score::mw::com::AsProxy<GetterOnlyEnabledInterface>;
using GetterOnlyEnabledSkeleton = score::mw::com::AsSkeleton<GetterOnlyEnabledInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_GETTER_ONLY_ENABLED_FIELD_H
