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

#ifndef SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_EXAMPLE_INTERFACE_H
#define SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_EXAMPLE_INTERFACE_H

#include "score/mw/com/types.h"
#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class ExampleInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Event<std::uint32_t> example_event{*this, "example_event"};
};

using ExampleInterfaceProxy = score::mw::com::AsProxy<ExampleInterface>;
using ExampleInterfaceSkeleton = score::mw::com::AsSkeleton<ExampleInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_EXAMPLE_INTERFACE_H
