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

#ifndef SCORE_MW_COM_TEST_FIELDS_GET_GETTER_ONLY_FIELD_H
#define SCORE_MW_COM_TEST_FIELDS_GET_GETTER_ONLY_FIELD_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

/// \brief Field interface with only WithGetter enabled.
///
/// The consumer uses Get() to poll the current field value.
/// The skeleton uses Update() to publish the value; the runtime registers a
/// Get handler automatically (via PR #438 / SkeletonField::RegisterGetHandler).
template <typename T>
class GetterOnlyInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, WithGetter> getter_only_field{*this, "getter_only_field"};
};

using GetterOnlyProxy = score::mw::com::AsProxy<GetterOnlyInterface>;
using GetterOnlySkeleton = score::mw::com::AsSkeleton<GetterOnlyInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_GET_GETTER_ONLY_FIELD_H
