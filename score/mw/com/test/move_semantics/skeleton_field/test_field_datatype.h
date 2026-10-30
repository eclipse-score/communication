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
#ifndef SCORE_MW_COM_TEST_SKELETON_FIELD_MOVE_SEMANTICS_TEST_FIELD_DATATYPE_H
#define SCORE_MW_COM_TEST_SKELETON_FIELD_MOVE_SEMANTICS_TEST_FIELD_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class SkeletonFieldMoveSemanticsInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, WithSetter, WithGetter, WithNotifier> moved_field_{*this, "moved_field"};
};

using SkeletonFieldMoveSemanticsProxy = score::mw::com::AsProxy<SkeletonFieldMoveSemanticsInterface>;
using SkeletonFieldMoveSemanticsSkeleton = score::mw::com::AsSkeleton<SkeletonFieldMoveSemanticsInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_SKELETON_FIELD_MOVE_SEMANTICS_TEST_FIELD_DATATYPE_H
