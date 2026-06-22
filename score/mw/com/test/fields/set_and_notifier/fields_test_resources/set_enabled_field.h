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

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_SET_ENABLED_FIELD_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_SET_ENABLED_FIELD_H

#include "score/mw/com/types.h"
#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class SetEnabledInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, WithSetter, WithNotifier> set_enabled_field{*this, "set_enabled_field"};
};

using SetEnabledProxy = score::mw::com::AsProxy<SetEnabledInterface>;
using SetEnabledSkeleton = score::mw::com::AsSkeleton<SetEnabledInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_SET_ENABLED_FIELD_H
