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

#ifndef OUR_NAME_SPACE_IMPL_TYPE_MULTI_DIM_VECTOR_H
#define OUR_NAME_SPACE_IMPL_TYPE_MULTI_DIM_VECTOR_H

#include "score/memory/shared/map.h"
#include "score/memory/shared/string.h"
#include "score/memory/shared/vector.h"
#include <our/name_space/impl_type_somevector.h>
#include <array>
#include <cstdint>

namespace our::name_space
{

using MultiDimVector = ::score::memory::shared::Vector<::our::name_space::SomeVector>;

}  // namespace our::name_space

#endif  // OUR_NAME_SPACE_IMPL_TYPE_MULTI_DIM_VECTOR_H
