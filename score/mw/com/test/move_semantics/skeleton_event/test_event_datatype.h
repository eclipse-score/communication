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
#ifndef SCORE_MW_COM_TEST_SKELETON_MOVE_SEMANTICS_TEST_EVENT_DATATYPE_H
#define SCORE_MW_COM_TEST_SKELETON_MOVE_SEMANTICS_TEST_EVENT_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class SkeletonMoveSemanticsInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Event<std::uint32_t> moved_event_{*this, "moved_event"};
};

using SkeletonMoveSemanticsProxy = score::mw::com::AsProxy<SkeletonMoveSemanticsInterface>;
using SkeletonMoveSemanticsSkeleton = score::mw::com::AsSkeleton<SkeletonMoveSemanticsInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_SKELETON_MOVE_SEMANTICS_TEST_EVENT_DATATYPE_H
