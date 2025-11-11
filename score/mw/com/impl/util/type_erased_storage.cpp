/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/util/type_erased_storage.h"

namespace score::mw::com::impl
{

bool operator==(const TypeErasedDataTypeInfo& lhs, const TypeErasedDataTypeInfo& rhs) noexcept
{
    return ((lhs.size == rhs.size) && (lhs.alignment == rhs.alignment));
}

}  // namespace score::mw::com::impl
