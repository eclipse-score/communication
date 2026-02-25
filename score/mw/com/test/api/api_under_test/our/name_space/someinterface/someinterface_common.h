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

#ifndef OUR_NAME_SPACE_SOMEINTERFACE_SOMEINTERFACE_COMMON_H
#define OUR_NAME_SPACE_SOMEINTERFACE_SOMEINTERFACE_COMMON_H

#include "our/name_space/impl_type_valueevttype.h"
#include "score/mw/com/impl/traits.h"

#include <vector>

namespace our::name_space::someinterface
{

template <typename Trait>
class SomeInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    static const std::vector<const char*> event_names;

    typename Trait::template Event<::our::name_space::ValueEvtType> Value{*this, event_names[0]};
};
template <typename Trait>
const std::vector<const char*> SomeInterface<Trait>::event_names{"Value"};

}  // namespace our::name_space::someinterface

#endif  // OUR_NAME_SPACE_SOMEINTERFACE_SOMEINTERFACE_COMMON_H
