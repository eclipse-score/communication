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
#ifndef SCORE_MW_COM_TEST_MOVE_SEMANTICS_SKELETON_METHOD_TEST_METHOD_DATATYPE_H
#define SCORE_MW_COM_TEST_MOVE_SEMANTICS_SKELETON_METHOD_TEST_METHOD_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class SkeletonMethodMoveInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Method<std::int32_t(std::int32_t, std::int32_t)> moved_method_{*this, "moved_method"};
};

using SkeletonMethodMoveProxy = score::mw::com::AsProxy<SkeletonMethodMoveInterface>;
using SkeletonMethodMoveSkeleton = score::mw::com::AsSkeleton<SkeletonMethodMoveInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_MOVE_SEMANTICS_SKELETON_METHOD_TEST_METHOD_DATATYPE_H
