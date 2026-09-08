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
#ifndef SCORE_MW_COM_TEST_ALL_SERVICE_ELEMENTS_ALL_SERVICE_ELEMENTS_DATATYPE_H
#define SCORE_MW_COM_TEST_ALL_SERVICE_ELEMENTS_ALL_SERVICE_ELEMENTS_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

using TestType = std::int32_t;

template <typename Trait>
class AllServiceElementsInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    // Note. The events, methods and fields are intentionally interleaved since they will be constructed in the order of
    // declaration and we want to test that the order of construction does not matter for the underlying implementation.
    typename Trait::template Event<TestType> event_1{*this, "event_1"};

    typename Trait::template Method<TestType(TestType, TestType)> with_in_args_and_return{*this,
                                                                                          "with_in_args_and_return"};

    typename Trait::template Field<TestType, WithGetter, WithNotifier> get_and_notifier_enabled_field{
        *this,
        "get_and_notifier_enabled_field"};

    typename Trait::template Method<void(TestType, TestType)> with_in_args_only{*this, "with_in_args_only"};

    typename Trait::template Field<TestType, WithGetter> get_only_enabled_field{*this, "get_only_enabled_field"};

    typename Trait::template Method<TestType()> with_return_only{*this, "with_return_only"};

    typename Trait::template Field<TestType, WithNotifier> notifier_only_enabled_field{*this,
                                                                                       "notifier_only_enabled_field"};

    typename Trait::template Method<void()> without_args_or_return{*this, "without_args_or_return"};

    typename Trait::template Field<TestType, WithSetter, WithGetter, WithNotifier>
        set_and_get_and_notifier_enabled_field{*this, "set_and_get_and_notifier_enabled_field"};

    typename Trait::template Field<TestType, WithSetter, WithGetter> set_and_get_enabled_field{
        *this,
        "set_and_get_enabled_field"};

    typename Trait::template Event<TestType> event_2{*this, "event_2"};

    typename Trait::template Field<TestType, WithSetter, WithNotifier> set_and_notifier_enabled_field{
        *this,
        "set_and_notifier_enabled_field"};
};

using AllServiceElementsProxy = score::mw::com::AsProxy<AllServiceElementsInterface>;
using AllServiceElementsSkeleton = score::mw::com::AsSkeleton<AllServiceElementsInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_ALL_SERVICE_ELEMENTS_ALL_SERVICE_ELEMENTS_DATATYPE_H
