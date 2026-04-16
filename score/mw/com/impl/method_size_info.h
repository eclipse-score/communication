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
#ifndef SCORE_MW_COM_IMPL_METHOD_SIZE_INFO_H
#define SCORE_MW_COM_IMPL_METHOD_SIZE_INFO_H

#include "score/mw/com/impl/data_type_meta_info.h"

#include <cstddef>
#include <optional>

namespace score::mw::com::impl
{

/// Binding-agnostic size info for a single method: in-args, return type, queue size.
/// in_args_type_info is empty for methods that take no arguments;
/// return_type_info is empty for methods with a void return type.
struct MethodSizeInfo
{
    std::optional<DataTypeMetaInfo> in_args_type_info;
    std::optional<DataTypeMetaInfo> return_type_info;
    std::size_t queue_size;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHOD_SIZE_INFO_H
