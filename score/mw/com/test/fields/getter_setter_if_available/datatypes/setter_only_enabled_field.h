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

#ifndef SCORE_MW_COM_TEST_FIELDS_GETTER_SETTER_IF_AVAILABLE_DATATYPES_SETTER_ONLY_ENABLED_FIELD_H
#define SCORE_MW_COM_TEST_FIELDS_GETTER_SETTER_IF_AVAILABLE_DATATYPES_SETTER_ONLY_ENABLED_FIELD_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class SetterOnlyEnabledInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, WithSetter, WithNotifier> setter_only_enabled_field{
        *this,
        "getter_setter_if_available_field"};
};

using SetterOnlyEnabledProxy = score::mw::com::AsProxy<SetterOnlyEnabledInterface>;
using SetterOnlyEnabledSkeleton = score::mw::com::AsSkeleton<SetterOnlyEnabledInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_GETTER_SETTER_IF_AVAILABLE_DATATYPES_SETTER_ONLY_ENABLED_FIELD_H
