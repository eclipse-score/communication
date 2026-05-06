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
#ifndef SCORE_MW_COM_IMPL_METHOD_IDENTIFIER_H
#define SCORE_MW_COM_IMPL_METHOD_IDENTIFIER_H

#include "score/mw/com/impl/method_type.h"

#include <string_view>
#include <tuple>

namespace score::mw::com::impl
{

/// \brief Lookup key for a method on a Proxy/Skeleton, formed from the method name and its MethodType.
/// A field's Get, a field's Set and a regular method may share the same name the type field disambiguates them.
struct UniqueMethodIdentifier
{
    std::string_view name;
    MethodType type;
};

inline bool operator==(const UniqueMethodIdentifier& lhs, const UniqueMethodIdentifier& rhs) noexcept
{
    return (lhs.name == rhs.name) && (lhs.type == rhs.type);
}

inline bool operator!=(const UniqueMethodIdentifier& lhs, const UniqueMethodIdentifier& rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator<(const UniqueMethodIdentifier& lhs, const UniqueMethodIdentifier& rhs) noexcept
{
    return std::tie(lhs.name, lhs.type) < std::tie(rhs.name, rhs.type);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHOD_IDENTIFIER_H
