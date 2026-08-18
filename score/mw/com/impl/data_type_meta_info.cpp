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
#include "score/mw/com/impl/data_type_meta_info.h"

#include "score/mw/com/impl/com_error.h"

namespace score::mw::com::impl
{

score::Result<score::memory::DataTypeSizeInfo> MakeDataTypeSizeInfo(const DataTypeMetaInfo& meta_info)
{
    const bool is_alignment_power_of_two =
        ((meta_info.alignment != 0U) && ((meta_info.alignment & (meta_info.alignment - 1U)) == 0U));
    if (!is_alignment_power_of_two)
    {
        return MakeUnexpected(ComErrc::kInvalidConfiguration,
                              "DataTypeMetaInfo alignment must be a non-zero power of two.");
    }

    const bool is_size_multiple_of_alignment = ((meta_info.size % meta_info.alignment) == 0U);
    if (!is_size_multiple_of_alignment)
    {
        return MakeUnexpected(ComErrc::kInvalidConfiguration,
                              "DataTypeMetaInfo size must be an integer multiple of its alignment.");
    }

    return score::memory::DataTypeSizeInfo{meta_info.size, meta_info.alignment};
}

}  // namespace score::mw::com::impl
