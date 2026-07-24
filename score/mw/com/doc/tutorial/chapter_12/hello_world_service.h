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
#ifndef SCORE_MW_COM_TUTORIAL_HELLO_WORLD_SERVICE_H
#define SCORE_MW_COM_TUTORIAL_HELLO_WORLD_SERVICE_H

#include "score/mw/com/types.h"

#include <array>

namespace score::mw::com::tutorial
{
template <typename Trait>
class HelloWorldInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    using FixedSizeString = std::array<char, 255>;
    typename Trait::template Event<FixedSizeString> message{*this, "message"};
};

using HelloWorldProxy = score::mw::com::AsProxy<score::mw::com::tutorial::HelloWorldInterface>;
using HelloWorldSkeleton = score::mw::com::AsSkeleton<score::mw::com::tutorial::HelloWorldInterface>;

}  // namespace score::mw::com::tutorial

#endif  // SCORE_MW_COM_TUTORIAL_HELLO_WORLD_SERVICE_H
