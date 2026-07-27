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

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_GET_AND_NOTIFIER_SET_GET_AND_NOTIFIER_ENABLED_FIELD_H
#define SCORE_MW_COM_TEST_FIELDS_SET_GET_AND_NOTIFIER_SET_GET_AND_NOTIFIER_ENABLED_FIELD_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

/// \brief Field interface with WithSetter, WithGetter, and WithNotifier all enabled.
///
/// The proxy can:
///  - Call Set() to request a value change (subject to the skeleton's set handler)
///  - Call Get() to poll the current committed field value
///  - Subscribe to receive notifications when the field value changes
template <typename T>
class SetGetAndNotifierInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, WithSetter, WithGetter, WithNotifier> set_get_and_notifier_enabled_field{
        *this,
        "set_get_and_notifier_enabled_field"};
};

using SetGetAndNotifierProxy = score::mw::com::AsProxy<SetGetAndNotifierInterface>;
using SetGetAndNotifierSkeleton = score::mw::com::AsSkeleton<SetGetAndNotifierInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_GET_AND_NOTIFIER_SET_GET_AND_NOTIFIER_ENABLED_FIELD_H
