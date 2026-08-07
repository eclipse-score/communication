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
#ifndef SCORE_MW_COM_TEST_FIELDS_MIXED_CRITICALITY_DATATYPES_MIXED_CRITICALITY_FIELDS_H
#define SCORE_MW_COM_TEST_FIELDS_MIXED_CRITICALITY_DATATYPES_MIXED_CRITICALITY_FIELDS_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class MixedCriticalityFieldsInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, WithNotifier> notifier_only_field{*this, "notifier_only_field"};
    typename T::template Field<std::int32_t, WithSetter, WithNotifier> setter_and_notifier_field{
        *this,
        "setter_and_notifier_field"};
    typename T::template Field<std::int32_t, WithGetter> getter_only_field{*this, "getter_only_field"};
    typename T::template Field<std::int32_t, WithGetter, WithNotifier> getter_and_notifier_field{
        *this,
        "getter_and_notifier_field"};
    typename T::template Field<std::int32_t, WithSetter, WithGetter> setter_and_getter_field{*this,
                                                                                             "setter_and_getter_field"};
    typename T::template Field<std::int32_t, WithSetter, WithGetter, WithNotifier> setter_getter_notifier_field{
        *this,
        "setter_getter_notifier_field"};
};

using MixedCriticalityFieldsProxy = score::mw::com::AsProxy<MixedCriticalityFieldsInterface>;
using MixedCriticalityFieldsSkeleton = score::mw::com::AsSkeleton<MixedCriticalityFieldsInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_MIXED_CRITICALITY_DATATYPES_MIXED_CRITICALITY_FIELDS_H
