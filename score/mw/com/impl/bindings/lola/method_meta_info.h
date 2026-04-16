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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHOD_META_INFO_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHOD_META_INFO_H

#include "score/mw/com/impl/data_type_meta_info.h"

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief meta-information about a method/its signature.
///          in_args_type_info_ is std::nullopt for methods that take no arguments.
///          return_type_info_ is std::nullopt for methods with a void return type.
class MethodMetaInfo
{
  public:
    MethodMetaInfo(const std::optional<impl::DataTypeMetaInfo> in_args_type_info,
                   const std::optional<impl::DataTypeMetaInfo> return_type_info)
        : in_args_type_info_(in_args_type_info), return_type_info_(return_type_info)
    {
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<impl::DataTypeMetaInfo> in_args_type_info_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<impl::DataTypeMetaInfo> return_type_info_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHOD_META_INFO_H
